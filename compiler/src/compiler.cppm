module;

#include <iostream>

export module zep.compiler;

import zep.common.source;
import zep.common.logger;
import zep.frontend.lexer;
import zep.frontend.parser;

export class Compiler {
  public:
    Compiler() = default;

    void compile(const Source& source) {
        auto logger = Logger(source);
        auto lexer = Lexer(source);
        auto parser = Parser(std::move(lexer), std::move(logger));

        auto program = parser.parse();
        program.dump();
    }
};
