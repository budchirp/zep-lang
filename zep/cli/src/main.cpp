#include <exception>
#include <filesystem>

import argman;
import zep.codegen.llvm.backend;
import zep.common.system.posix;
import zep.common.system.process;
import zep.build;
import zep.workspace.toolchain;
import zep.cli.commands.build;
import zep.cli.commands.compile;
import zep.cli.commands.fetch;
import zep.cli.commands.lsp;
import zep.cli.commands.test;
import zep.common.logger;

class RootCommand : public argman::Command {
  private:
    BuildCommand build_command;
    CompileCommand compile_command;
    FetchCommand fetch_command;
    LspCommand lsp_command;
    TestCommand test_command;

  public:
    explicit RootCommand(Builder& builder, ProcessRunner& process_runner,
                         const std::filesystem::path& standard_library)
        : build_command(builder), compile_command(builder), fetch_command(process_runner),
          lsp_command(standard_library), test_command(builder) {}

  private:
    argman::Command::Info info() override {
        return {.name = "zep",
                .description = "The Zep programming language project builder",
                .commands = {&build_command, &compile_command, &fetch_command,
                             &lsp_command, &test_command}};
    }

    int execute() override {
        Logger::print("Use --help for available commands.\n");
        return 0;
    }
};

int main(int argument_count, char* arguments[]) {
    LLVMBackend backend;
    Toolchain environment = Toolchain::discover();
    if (environment.standard_library.empty()) {
        environment.standard_library = ZEP_STD_SOURCE_DIR;
    }

    const auto standard_library = environment.standard_library;
    PosixProcessRunner process_runner;
    Builder builder(backend, std::move(environment), process_runner);
    RootCommand root(builder, process_runner, standard_library);
    argman::CommandLineParser parser(root);

    try {
        return parser.parse(argument_count, arguments);
    } catch (const std::exception& exception) {
        Logger::print_stderr("zep: error: ", exception.what(), "\n");
        return 1;
    }
}
