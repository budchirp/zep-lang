module;

#include <cstdlib>

export module zep.driver;

import zep.common.source;
import zep.common.logger;
import zep.frontend.lexer;
import zep.frontend.parser;
import zep.frontend.sema.context;
import zep.frontend.sema.type.checker;
import zep.frontend.debug.ast_dumper;
import zep.hir;
import zep.hir.ir;
import zep.hir.debug.dumper;
import zep.frontend.debug.sema_dumper;

export class Driver {
  private:
  public:
    Driver() = default;

    void run(const Source& source) {
        Context context(source);

        Parser parser(context, Lexer(context.source.content));
        auto program = parser.parse();

        TypeChecker type_checker(context);
        type_checker.check(program);

        if (context.diagnostics.has_errors()) {
            context.diagnostics.print(context.logger);
            std::exit(1);
        }

        // AstDumper ast_dumper;
        // ast_dumper.dump_program(program);

        auto hir_program = HIRProgram();
        HIRBuilder hir_builder(hir_program.arena, context.types, context.env);
        hir_program = hir_builder.lower(program);

        HIRDumper hir_dumper;
        hir_dumper.dump_program(hir_program);

        Logger::print("\n");
        SemaDumper sema_dumper;
        sema_dumper.dump_scope(context.env.global_scope);
    }
};
