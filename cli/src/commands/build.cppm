module;

#include <filesystem>
#include <string>

export module zep.cli.commands.build;

import argman;
import zep.codegen.options;
import zep.common.logger;
import zep.common.target;
import zep.build;
import zep.cli.commands.codegen;

export class BuildCommand : public argman::Command {
  private:
    Builder& builder;

  public:
    explicit BuildCommand(Builder& builder) : builder(builder) {}

    argman::Command::Info info() override {
        return {.name = "build",
                .description = "Build a Zep project",
                .options = CodegenArguments::options({
                    argman::Option("project", "Project root directory", std::string("")),
                })};
    }

    int execute() override {
        auto options = CodegenArguments::parse(get<int>("optimization"), get<bool>("emit-ir"));

        if (!options.has_value()) {
            return 1;
        }

        auto project = get<std::string>("project");
        auto path =
            project.empty() ? std::filesystem::current_path() : std::filesystem::path(project);
        if (!builder.build(path, options->optimization, options->debug_output)) {
            return 1;
        }

        return 0;
    }
};
