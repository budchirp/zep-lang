module;

#include <cstdlib>

export module zep.driver;

import zep.common.source;
import zep.common.logger;
import zep.common.context;
import zep.frontend.lexer;
import zep.frontend.parser;
import zep.frontend.sema.context;
import zep.frontend.sema.type.checker;
import zep.frontend.debug.ast_dumper;
import zep.hir.builder;
import zep.hir.node.program;
import zep.hir.debug.dumper;
import zep.codegen;

export class Driver {
  private:
  public:
    Driver() = default;

    void run(const Source& source) {
        Context context(source);
        SemaContext sema;

        Parser parser(context, sema, Lexer(context.source.content));
        auto program = parser.parse();

        TypeChecker type_checker(context, sema);
        type_checker.check(program);

        if (context.diagnostics.has_errors()) {
            context.diagnostics.print(context.logger);
            std::exit(1);
        }

        HIRBuilder hir_builder(context, sema);
        auto hir_program = hir_builder.lower(program);

        Codegen codegen;
        codegen.generate(hir_program);
    }
};
