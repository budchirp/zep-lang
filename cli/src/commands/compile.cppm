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

export class CompileCommand : public argman::Command {
  private:
    Builder& builder;

  public:
    explicit CompileCommand(Builder& builder) : builder(builder) {}

    argman::Command::Info info() override {
        return {.name = "compile",
                .description = "Compile a single Zep source file",
                .options = {
                    argman::Option("input", "Input Zep source file", std::string("")),
                    argman::Option("output", "Output object file", std::string(""), false, {"o"}),
                    argman::Option("target",
                                   "Target triple. Valid: x86_64-unknown-linux-gnu, "
                                   "aarch64-unknown-linux-gnu, aarch64-apple-darwin",
                                   std::string("")),
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

        auto input = get<std::string>("input");
        if (input.empty()) {
            Logger::print_stderr("zep: error: missing --input\n");
            return 1;
        }

        auto input_path = std::filesystem::absolute(input);
        auto output = get<std::string>("output");
        if (output.empty()) {
            output = (input_path.parent_path() / (input_path.stem().string() + ".o")).string();
        }

        auto triple = get<std::string>("target");
        TargetInfo target(triple.empty() ? TargetInfo::host_triple() : triple);

        if (!builder.compile(input_path, output,
                             CodegenOptions(target, optimization_level.value(),
                                            get<bool>("emit-ir") ? DebugOutput::Type::IR
                                                                 : DebugOutput::Type::None))) {
            return 1;
        }

        Logger::print("compiled '", input_path.string(), "' -> ", output, "\n");
        return 0;
    }
};
