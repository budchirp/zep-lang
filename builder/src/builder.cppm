module;

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

export module zep.build;

import zep.codegen.api;
import zep.common.diagnostic.collection;
import zep.common.logger;
import zep.common.system.command;
import zep.common.system.process;
import zep.common.target;
import zep.compiler;
import zep.compiler.unit;
import zep.workspace.manifest;
import zep.workspace.module_path;
import zep.workspace.project;
import zep.workspace.toolchain;

export class Builder {
  private:
    CodegenBackend& backend;
    Toolchain toolchain;
    ProcessRunner& process_runner;

    static std::filesystem::path object_path(const std::filesystem::path& build_directory,
                                             const Module& module) {
        auto relative = module.path.path();
        relative.replace_extension(".o");
        return build_directory / "objs" / relative;
    }

    bool generate(const HIRProgram& program, const std::filesystem::path& output,
                  const CodegenOptions& options) {
        std::filesystem::create_directories(output.parent_path());
        auto result = backend.generate(program, output, options);
        if (result.has_value()) {
            return true;
        }

        Logger::print_stderr("zep: error: ", result.error(), "\n");
        return false;
    }

    bool link(const std::vector<std::filesystem::path>& objects,
              const std::filesystem::path& output, const TargetInfo& target,
              const std::vector<std::string>& linker_arguments,
              const std::filesystem::path& working_directory) {
        std::vector<std::string> arguments;
        arguments.reserve(objects.size() + linker_arguments.size() + 7);
        arguments.push_back(toolchain.compiler.string());
        arguments.push_back("-fuse-ld=lld");
        arguments.push_back("--target=" + target.triple);

        for (const auto& object : objects) {
            arguments.push_back(object.string());
        }

        arguments.insert(arguments.end(), linker_arguments.begin(), linker_arguments.end());
        arguments.push_back("-Wl,--allow-multiple-definition");
        arguments.push_back("-o");
        arguments.push_back(output.string());

        std::filesystem::create_directories(output.parent_path());
        auto result = process_runner.run(Command(std::move(arguments), working_directory));
        if (result.succeeded()) {
            return true;
        }

        Logger::print_stderr("zep: error: ", result.stderr_text, "\n");
        return false;
    }

  public:
    Builder(CodegenBackend& backend, Toolchain toolchain, ProcessRunner& process_runner)
        : backend(backend), toolchain(std::move(toolchain)), process_runner(process_runner) {}

    bool compile(const std::filesystem::path& input, const std::filesystem::path& output,
                 const CodegenOptions& options) {
        if (!options.target.is_supported()) {
            return Logger::fail("unsupported target '", options.target.triple, "'");
        }

        auto project = Project::single_file(input, toolchain);
        auto module_path = project.module(input);
        if (!module_path.has_value()) {
            return Logger::fail("could not resolve input module");
        }

        Diagnostics diagnostics;
        Compiler compiler(options.target);
        auto* module =
            compiler.analyze(*project.root_package, std::move(*module_path), diagnostics, input);
        if (module == nullptr || diagnostics.has_errors()) {
            diagnostics.print();
            return false;
        }

        auto program = compiler.lower(*module, diagnostics);
        if (program == nullptr || diagnostics.has_errors()) {
            diagnostics.print();
            return false;
        }

        return generate(*program, output, options);
    }

    bool build(const std::filesystem::path& path, OptimizationLevel::Type optimization,
               DebugOutput::Type debug_output) {
        Diagnostics project_diagnostics;
        auto project = Project::open(path, project_diagnostics, toolchain);
        if (!project.has_value()) {
            if (project_diagnostics.entries.empty()) {
                return Logger::fail("could not find zep.json");
            }
            project_diagnostics.print();
            return false;
        }

        const auto& manifest = project->root_package->manifest;
        for (const auto& build_target : manifest.targets) {
            TargetInfo target(build_target.triple);
            if (!target.is_supported()) {
                return Logger::fail("unsupported target '", build_target.triple, "'");
            }

            Diagnostics diagnostics;
            Compiler compiler(target);
            auto entry_name = manifest.type == Manifest::Type::Kind::Library ? "lib" : "main";
            auto* entry = compiler.analyze(*project->root_package,
                                           ModulePath::from_string(entry_name), diagnostics);
            if (entry == nullptr || diagnostics.has_errors()) {
                diagnostics.print();
                return false;
            }

            auto build_root = project->root_directory / "build";
            auto build_directory =
                manifest.targets.size() > 1 ? build_root / build_target.triple : build_root;
            std::vector<std::filesystem::path> objects;
            objects.reserve(compiler.modules().size());
            for (auto* module : compiler.modules()) {
                auto program = compiler.lower(*module, diagnostics);
                if (program == nullptr || diagnostics.has_errors()) {
                    diagnostics.print();
                    return false;
                }

                auto output = object_path(build_directory, *module);
                if (!generate(*program, output,
                              CodegenOptions(target, optimization, debug_output))) {
                    return false;
                }
                objects.push_back(std::move(output));
            }

            if (manifest.type == Manifest::Type::Kind::Executable &&
                !link(objects, build_directory / manifest.name, target,
                      build_target.linker_arguments, project->root_directory)) {
                return false;
            }
        }

        Logger::print("built '", manifest.name, "' -> ",
                      (project->root_directory / "build").string(), "\n");
        return true;
    }
};

export class DependencyFetcher {
  private:
    ProcessRunner& process_runner;

    bool run(const std::vector<std::string>& arguments,
             const std::filesystem::path& working_directory) const {
        auto result = process_runner.run(Command(arguments, working_directory));
        if (!result.succeeded()) {
            Logger::print_stderr("zep: error: ", result.stderr_text, "\n");
        }
        return result.succeeded();
    }

  public:
    explicit DependencyFetcher(ProcessRunner& process_runner) : process_runner(process_runner) {}

    bool fetch(const std::filesystem::path& path) const {
        ManifestReader reader;
        auto manifest_path = reader.find(path);
        if (!manifest_path.has_value()) {
            return Logger::fail("could not find zep.json");
        }

        auto manifest = reader.read(*manifest_path);
        if (!manifest.has_value()) {
            return Logger::fail("could not read zep.json");
        }

        auto root = manifest_path->parent_path();
        for (const auto& [name, dependency] : manifest->libs) {
            if (dependency.source != PackageSource::Type::Git) {
                continue;
            }

            auto target = root / "build/libs" / name / dependency.version;
            std::filesystem::create_directories(target.parent_path());
            if (!std::filesystem::exists(target)) {
                if (!run({"git", "clone", dependency.location, target.string()}, root)) {
                    return false;
                }
            } else if (!run({"git", "-C", target.string(), "fetch", "origin"}, root) ||
                       !run({"git", "-C", target.string(), "pull", "--ff-only"}, root)) {
                return false;
            }
            Logger::print("fetched ", name, " -> ", target.string(), "\n");
        }
        return true;
    }
};

export class Installer {
  public:
    static bool install(const std::filesystem::path& executable,
                        const std::filesystem::path& standard_source,
                        const std::filesystem::path& destination, const std::string& version) {
        auto binary_directory = destination / "bin";
        auto standard = destination / "libs/std" / version;
        std::filesystem::create_directories(binary_directory);
        std::filesystem::create_directories(standard.parent_path());
        std::filesystem::copy_file(executable, binary_directory / "zep",
                                   std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove_all(standard);
        std::filesystem::create_directories(standard);
        std::filesystem::copy_file(standard_source / "zep.json", standard / "zep.json",
                                   std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy(standard_source / "src", standard / "src",
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);
        Logger::print("installed zep to ", (binary_directory / "zep").string(), "\n");
        Logger::print("installed std to ", standard.string(), "\n");
        return true;
    }
};
