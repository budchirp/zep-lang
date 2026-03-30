module;

#include <cstdlib>
#include <memory>
#include <utility>

export module zep.driver;

import zep.common.source;
import zep.common.logger;
import zep.frontend.lexer;
import zep.frontend.parser;
import zep.frontend.sema.checker.type_checker;
import zep.frontend.sema.context;
import zep.hir;

export class Driver {
  public:
    Driver() = default;

    void run(const Source& source) {
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

        context.env.dump();

        HIRBuilder hir_builder(context);
        auto hir_program = hir_builder.lower(program);
        hir_program.dump();
    }
};
