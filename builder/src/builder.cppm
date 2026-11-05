module;

#include <filesystem>
#include <utility>
#include <vector>

export module zep.build;

import zep.build.objects;
import zep.build.linker;
import zep.codegen.api;
import zep.common.diagnostic.collection;
import zep.common.logger;
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
    Toolchain toolchain;
    Objects objects;
    Linker linker;

  public:
    Builder(Backend& backend, Toolchain toolchain, ProcessRunner& process_runner)
        : toolchain(std::move(toolchain)), objects(backend),
          linker(this->toolchain.compiler, process_runner) {}

    bool compile(const std::filesystem::path& input, const std::filesystem::path& output,
                 const EmitOptions& options) {
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

        return objects.generate(compiler, *module, diagnostics, output, options);
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
            const auto* entry_name = manifest.type == Manifest::Type::Kind::Library ? "lib" : "main";
            auto* entry = compiler.analyze(*project->root_package,
                                           ModulePath::from_string(entry_name), diagnostics);

            if (entry == nullptr || diagnostics.has_errors()) {
                diagnostics.print();

                return false;
            }

            auto build_directory = project->root_directory / "build";

            if (manifest.targets.size() > 1) {
                build_directory /= build_target.triple;
            }

            EmitOptions options(target, optimization, debug_output);
            std::vector<std::filesystem::path> outputs;
            outputs.reserve(compiler.modules.size());

            for (auto* module : compiler.modules) {
                auto relative = module->path.path();
                relative.replace_extension(".o");

                auto output = build_directory / "objs" / relative;

                if (!objects.generate(compiler, *module, diagnostics, output, options)) {
                    return false;
                }

                outputs.push_back(std::move(output));
            }

            if (manifest.type == Manifest::Type::Kind::Executable &&
                !linker.link(outputs, build_directory / manifest.name, target,
                             build_target.linker_arguments, project->root_directory)) {
                return false;
            }
        }

        Logger::print("built '", manifest.name, "' -> ",
                      (project->root_directory / "build").string(), "\n");

        return true;
    }
};
