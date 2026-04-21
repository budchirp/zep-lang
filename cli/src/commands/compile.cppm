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
                }};
    }

    void execute() override {
        auto filename = get<std::string>("input");
        if (filename.empty()) {
            throw std::invalid_argument("no input file specified (use --input <file>)");
        }

        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("could not open file '" + filename + "'");
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();
        auto content = buffer.str();

        Source source(filename, content);

        Driver driver;
        driver.run(source);
    }
};
