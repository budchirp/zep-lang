module;

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

export module zep.build.service;

import zep.build.artifact;
import zep.build.executor;
import zep.build.link;
import zep.build.plan;
import zep.common.system.process;
import zep.codegen.api;
import zep.common.diagnostic.collection;
import zep.common.logger;
import zep.common.target;
import zep.compiler;
import zep.compiler.unit;
import zep.workspace.toolchain;
import zep.workspace.manifest;
import zep.workspace.module_path;
import zep.workspace.package;
import zep.workspace.package.graph;
import zep.workspace.project;
import zep.common.system.command;
import zep.hir.program;

export class BuildService {
  private:
    CodegenBackend& backend;
    Toolchain environment;
    ProcessRunner& process_runner;

    static std::filesystem::path object_path(const std::filesystem::path& build_root,
                                             const Module& module) {
        auto relative = module.path.path();
        relative.replace_extension(".o");
        return build_root / "objs" / relative;
    }

    bool run_plan(BuildPlan& plan) {
        auto result = BuildExecutor::execute(plan, backend, process_runner);
        if (result.succeeded()) {
            return true;
        }
        for (const auto& failure : result.failures) {
            Logger::print_stderr("zep: error: ", failure.message, "\n");
        }
        return false;
    }

  public:
    BuildService(CodegenBackend& backend, Toolchain environment, ProcessRunner& process_runner)
        : backend(backend), environment(std::move(environment)), process_runner(process_runner) {}

    bool compile_object(const std::filesystem::path& input, const std::filesystem::path& output,
                        const TargetInfo& target, OptimizationLevel::Type optimization,
                        bool verbose) {
        auto root = input.parent_path().parent_path();
        PackageGraph packages;
        auto& package =
            packages.add(Manifest("main", "0.1.0", Manifest::Type::Kind::Executable, {}, {}),
                         PackageSource::Type::Workspace, root);
        if (!environment.standard_library.empty()) {
            auto& standard =
                packages.add(Manifest("std", "0.0.1", Manifest::Type::Kind::Library, {}, {}),
                             PackageSource::Type::Global, environment.standard_library);
            package.dependencies.push_back(&standard);
        }

        Diagnostics diagnostics;
        Compiler compiler(target);
        auto* module =
            compiler.load(package, ModulePath::from_string(input.stem().string()), diagnostics);
        if (module == nullptr || diagnostics.has_errors() || !compiler.check()) {
            diagnostics.print();
            return false;
        }

        auto program = compiler.lower(*module);
        if (program == nullptr) {
            return false;
        }

        std::filesystem::create_directories(output.parent_path());
        BuildPlan plan;
        plan.add_compile(program, output,
                         CodegenOptions(target, optimization,
                                        verbose ? DebugOutput::Type::IR : DebugOutput::Type::None));
        return run_plan(plan);
    }

    bool build(const std::filesystem::path& root, CodegenOptions options, bool verbose) {
        static_cast<void>(verbose);
        ManifestReader reader;
        auto manifest = reader.read(root / "zep.json");
        if (!manifest.has_value()) {
            return Logger::fail("could not find zep.json");
        }
        for (const auto& target : manifest->targets) {
            TargetInfo target_info(target.triple);
            if (!target_info.is_supported()) {
                return Logger::fail("unsupported target '", target.triple, "'");
            }
            Diagnostics diagnostics;
            auto project = Project::open(root, diagnostics, environment);
            if (!project.has_value()) {
                diagnostics.print();
                return false;
            }
            auto* package = project->root_package;
            auto entry_name = manifest->type == Manifest::Type::Kind::Library ? "lib" : "main";
            Compiler compiler(target_info);
            auto* entry = compiler.load(*package, ModulePath::from_string(entry_name), diagnostics);
            if (entry == nullptr || diagnostics.has_errors() || !compiler.check()) {
                diagnostics.print();
                return false;
            }

            std::vector<std::shared_ptr<const HIRProgram>> programs;
            std::vector<ObjectArtifact> objects;
            BuildPlan plan;
            auto build_root = root / "build";
            programs.reserve(compiler.ordering().size());
            objects.reserve(compiler.ordering().size());
            for (auto* module : compiler.ordering()) {
                programs.push_back(compiler.lower(*module));
                if (programs.back() == nullptr) {
                    return false;
                }

                auto path = object_path(build_root, *module);
                std::filesystem::create_directories(path.parent_path());
                plan.add_compile(programs.back(), path, options);
                objects.emplace_back(path);
            }
            if (manifest->type == Manifest::Type::Kind::Executable) {
                auto output = build_root / manifest->name;
                std::filesystem::create_directories(output.parent_path());
                auto arguments = target.linker_arguments;
                arguments.insert(arguments.begin(), "-Wl,--allow-multiple-definition");
                plan.add_link(LinkPlanner::executable(objects, ExecutableArtifact(output),
                                                      target_info, arguments, root),
                              ExecutableArtifact(output));
            }
            if (!run_plan(plan)) {
                return false;
            }
            Logger::print("built '", manifest->name, "' -> ",
                          (build_root / manifest->name).string(), "\n");
        }
        return true;
    }
};

export class DependencyFetcher {
  private:
    ProcessRunner& process_runner;
    std::filesystem::path root;

    bool run(const std::vector<std::string>& arguments) const {
        auto result = process_runner.run(Command(arguments, root));
        if (!result.succeeded()) {
            Logger::print_stderr("zep: error: ", result.stderr_text, "\n");
        }
        return result.succeeded();
    }

  public:
    DependencyFetcher(ProcessRunner& process_runner, std::filesystem::path root)
        : process_runner(process_runner), root(std::move(root)) {}

    bool fetch(const Manifest& manifest) const {
        for (const auto& [name, dependency] : manifest.libs) {
            if (dependency.source != PackageSource::Type::Git) {
                continue;
            }
            auto target = root / "build/libs" / name / dependency.version;
            std::filesystem::create_directories(target.parent_path());
            if (!std::filesystem::exists(target)) {
                if (!run({"git", "clone", dependency.location, target.string()})) {
                    return false;
                }
            } else if (!run({"git", "-C", target.string(), "fetch", "origin"}) ||
                       !run({"git", "-C", target.string(), "pull", "--ff-only"})) {
                return false;
            }
            Logger::print("fetched ", name, " -> ", target.string(), "\n");
        }
        return true;
    }
};

export class InstallService {
  public:
    static bool install(const std::filesystem::path& executable,
                        const std::filesystem::path& standard_source,
                        const std::filesystem::path& destination, const std::string& version) {
        auto bin = destination / "bin";
        auto standard = destination / "libs/std" / version;
        std::filesystem::create_directories(bin);
        std::filesystem::create_directories(standard.parent_path());
        std::filesystem::copy_file(executable, bin / "zep",
                                   std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove_all(standard);
        std::filesystem::create_directories(standard);
        std::filesystem::copy_file(standard_source / "zep.json", standard / "zep.json",
                                   std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy(standard_source / "src", standard / "src",
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);
        Logger::print("installed zep to ", (bin / "zep").string(), "\n");
        Logger::print("installed std to ", standard.string(), "\n");
        return true;
    }
};
