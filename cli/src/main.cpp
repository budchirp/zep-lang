#include <cstdio>
#include <print>

import argman;
import zep.cli.commands.compile;

class RootCommand : public argman::Command {
  private:
    CompileCommand compile_command;

  public:
    argman::Command::Info info() override {
        return {.name = "zep",
                .description = "The Zep programming language compiler",
                .commands = {&compile_command}};
    }

    void execute() override { std::println("Use --help for available commands."); }
};

int main(int argc, char* argv[]) {
    RootCommand root;
    argman::CommandLineParser parser(root);

    try {
        parser.parse(argc, argv);
    } catch (const std::exception& exception) {
        std::println(stderr, "zep: error: {}", exception.what());
        return 1;
    }

    return 0;
}
