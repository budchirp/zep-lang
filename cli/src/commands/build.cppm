module;

#include <filesystem>
#include <string>

export module zep.cli.commands.build;

import argman;
import zep.codegen.options;
import zep.common.logger;
import zep.common.target;
import zep.build;

export class BuildCommand : public argman::Command {
  private:
    Builder& builder;

  public:
    explicit BuildCommand(Builder& builder) : builder(builder) {}

    argman::Command::Info info() override {
        return {.name = "build",
                .description = "Build a Zep project",
                .options = {
                    argman::Option("project", "Project root directory", std::string("")),
                    argman::Option("emit-ir", "Print LLVM IR after code generation", false, true),
                    argman::Option("optimization", "Optimization level (0, 1, 2, or 3)", 0, false,
                                   {"O"}),
                }};
    }

    int execute() override {
        auto optimization_level = OptimizationLevel::from_int(get<int>("optimization"));
        if (!optimization_level.has_value()) {
            Logger::print_stderr("zep: error: optimization level must be 0, 1, 2, or 3\n");
            return 1;
        }

        auto project = get<std::string>("project");
        auto path =
            project.empty() ? std::filesystem::current_path() : std::filesystem::path(project);
        if (!builder.build(path, optimization_level.value(),
                           get<bool>("emit-ir") ? DebugOutput::Type::IR
                                                : DebugOutput::Type::None)) {
            return 1;
        }

        return 0;
    }
};
