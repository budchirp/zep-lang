module;

#include <cstdlib>
#include <memory>
#include <utility>

export module zep.compiler;

import zep.common.source;
import zep.common.logger;
import zep.frontend.lexer;
import zep.frontend.parser;
import zep.checker.type_checker;
import zep.checker.context;

export class Compiler {
  public:
    Compiler() = default;

    void compile(const Source& source) {
        Context context(source);

        Lexer lexer(context.source.content);
        Parser parser(lexer, context.logger);

        auto program = parser.parse();

        TypeChecker type_checker(context);
        type_checker.check(program);

        if (context.diagnostics.has_errors()) {
            context.diagnostics.print(context.logger);
            std::exit(1);
        }

        program.dump();
    }
};
