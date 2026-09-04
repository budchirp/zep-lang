module;

#include <filesystem>
#include <string>

export module zep.cli.commands.build;

import argman;
import zep.codegen.options;
import zep.common.logger;
import zep.common.target;
import zep.build.service;

export class BuildCommand : public argman::Command {
  private:
    BuildService& service;

  public:
    explicit BuildCommand(BuildService& service) : service(service) {}

    argman::Command::Info info() override {
        return {
            .name = "build",
            .description = "Build a Zep project",
            .options = {
                argman::Option("project", "Project root directory", std::string("")),
                argman::Option("verbose", "Print the LLVM IR after codegen", false, true, {"v"}),
                argman::Option("optimization", "Optimization level (0, 1, 2, or 3)", 0, false,
                               {"o"}),
            }};
    }

    int execute() override {
        auto optimization_level = OptimizationLevel::from_int(get<int>("optimization"));
        if (!optimization_level.has_value()) {
            Logger::print_stderr("zep: error: optimization level must be 0, 1, 2, or 3\n");
            return 1;
        }

        auto project = get<std::string>("project");
        auto root =
            project.empty() ? std::filesystem::current_path() : std::filesystem::path(project);
        if (project.empty()) {
            while (!std::filesystem::is_regular_file(root / "zep.json") &&
                   root != root.parent_path()) {
                root = root.parent_path();
            }
        }
        if (!std::filesystem::is_regular_file(root / "zep.json")) {
            Logger::print_stderr("zep: error: could not find zep.json\n");
            return 1;
        }
        if (!service.build(root,
                           CodegenOptions(TargetInfo(), optimization_level.value(),
                                          get<bool>("verbose") ? DebugOutput::Type::IR
                                                               : DebugOutput::Type::None),
                           get<bool>("verbose"))) {
            return 1;
        }

        return 0;
    }
};
