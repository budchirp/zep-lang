module;

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

export module zep.cli.commands.compile;

import argman;
import zep.common.source;
import zep.driver;

export class CompileCommand : public argman::Command {
  private:
  public:
    argman::Command::Info info() override {
        return {.name = "compile",
                .description = "Compile a Zep source file",
                .options = {
                    argman::Option("input", "Input source file", std::string("")),
                    argman::Option("libs", "List of external .zep source files",
                                   std::vector<std::string>{}),
                    argman::Option("objs", "List of external object files (.o) to link",
                                   std::vector<std::string>{}),
                    argman::Option("out", "Output binary name/path", std::string("program")),
                }};
    }

    void execute() override {
        CompileOptions options;
        options.input = get<std::string>("input");
        options.libs = get<std::vector<std::string>>("libs");
        options.objs = get<std::vector<std::string>>("objs");
        options.out = get<std::string>("out");

        if (options.input.empty()) {
            throw std::invalid_argument("no input file specified (use --input <file>)");
        }

        Driver driver;
        driver.run(options);
    }
};
