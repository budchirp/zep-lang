module;

#include <filesystem>
#include <string>

export module zep.cli.commands.test;

import argman;
import zep.build;
import zep.cli.commands.codegen;
import zep.common.logger;
import zep.common.paths;

export class TestCommand : public argman::Command {
  private:
    Builder& builder;

  public:
    explicit TestCommand(Builder& builder) : builder(builder) {}

    argman::Command::Info info() override {
        return {.name = "test",
                .description = "Build and run named Zep tests in isolated processes",
                .options = CodegenArguments::options({
                    argman::Option("project", "Project root directory", std::string("")),
                    argman::Option("input", "Single input source file", std::string("")),
                    argman::Option("filter", "Qualified test name substring", std::string("")),
                    argman::Option("list", "List matching tests without building", false, true),
                })};
    }

    int execute() override {
        const auto project = get<std::string>("project");
        const auto input = get<std::string>("input");

        if (!project.empty() && !input.empty()) {
            Logger::print_stderr("zep: error: --project and --input cannot be combined\n");

            return 1;
        }

        const auto options = CodegenArguments::parse(get<int>("optimization"), get<bool>("emit-ir"));

        if (!options.has_value()) {
            return 1;
        }

        return builder.test(project.empty() ? std::filesystem::current_path() : Paths::absolute(project),
                            input.empty() ? std::filesystem::path() : Paths::absolute(input),
                            get<std::string>("filter"), get<bool>("list"), *options) ? 0 : 1;
    }
};
