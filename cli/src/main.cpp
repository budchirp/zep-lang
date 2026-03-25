#include <exception>

import argman;
import zep.cli.commands.compile;
import zep.common.logger;

class RootCommand : public argman::Command {
  private:
    CompileCommand compile_command;

  public:
    argman::Command::Info info() override {
        return {.name = "zep",
                .description = "The Zep programming language compiler",
                .commands = {&compile_command}};
    }

    void execute() override { Logger::print("Use --help for available commands.\n"); }
};

int main(int argc, char* argv[]) {
    RootCommand root;
    argman::CommandLineParser parser(root);

    try {
        parser.parse(argc, argv);
    } catch (const std::exception& exception) {
        Logger::print_stderr("zep: error: ", exception.what(), "\n");
        return 1;
    }

    return 0;
}
