module;

#include <filesystem>
#include <iostream>
#include <utility>

export module zep.cli.commands.lsp;

import argman;
import zep.lsp.server;

export class LspCommand : public argman::Command {
  private:
    std::filesystem::path standard_library;

  public:
    explicit LspCommand(std::filesystem::path standard_library)
        : standard_library(std::move(standard_library)) {}

    argman::Command::Info info() override {
        return {
            .name = "lsp",
            .description = "Start the Zep Language Server Protocol (LSP) server",
        };
    }

    int execute() override {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(nullptr);

        Server server(std::cin, std::cout, standard_library);
        return server.run();
    }
};
