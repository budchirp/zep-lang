module;

#include <filesystem>
#include <string>
#include <utility>

export module zep.cli.commands.compile;

import argman;
import zep.codegen.options;
import zep.common.logger;
import zep.common.target;
import zep.build;
import zep.cli.commands.codegen;
import zep.common.paths;

export class CompileCommand : public argman::Command {
  private:
    Builder& builder;

  public:
    explicit CompileCommand(Builder& builder) : builder(builder) {}

    argman::Command::Info info() override {
        return {.name = "compile",
                .description = "Compile a single Zep source file",
                .options = CodegenArguments::options({
                    argman::Option("input", "Input Zep source file", std::string("")),
                    argman::Option("output", "Output object file", std::string(""), false, {"o"}),
                    argman::Option("target",
                                   "Target triple. Valid: x86_64-unknown-linux-gnu, "
                                   "aarch64-unknown-linux-gnu, aarch64-apple-darwin",
                                   std::string("")),
                })};
    }

    int execute() override {
        auto options = CodegenArguments::parse(get<int>("optimization"), get<bool>("emit-ir"),
                                               get<std::string>("target"));

        if (!options.has_value()) {
            return 1;
        }

        auto input = get<std::string>("input");
        if (input.empty()) {
            Logger::print_stderr("zep: error: missing --input\n");
            return 1;
        }

        auto input_path = Paths::absolute(input);
        auto output = get<std::string>("output");
        if (output.empty()) {
            output = (input_path.parent_path() / (input_path.stem().string() + ".o")).string();
        }

        if (!builder.compile(input_path, output, *options)) {
            return 1;
        }

        Logger::print("compiled '", input_path.string(), "' -> ", output, "\n");
        return 0;
    }
};
