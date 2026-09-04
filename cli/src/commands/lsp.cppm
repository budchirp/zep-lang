module;

#include <iostream>

export module zep.cli.commands.lsp;

import argman;
import zep.lsp.server;

export class LspCommand : public argman::Command {
  public:
    LspCommand() = default;

    argman::Command::Info info() override {
        return {
            .name = "lsp",
            .description = "Start the Zep Language Server Protocol (LSP) server",
        };
    }

    int execute() override {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(nullptr);

        Server server(std::cin, std::cout);
        return server.run();
    }
};
