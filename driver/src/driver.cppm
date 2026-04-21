module;

#include <cstdlib>

export module zep.driver;

import zep.common.source;
import zep.common.logger;
import zep.frontend.lexer;
import zep.frontend.parser;
import zep.frontend.sema.context;
import zep.frontend.sema.checker;
import zep.frontend.debug.ast_dumper;

export class Driver {
  private:
  public:
    Driver() = default;

    void run(const Source& source) {
        Context context(source);

        Parser parser(context, Lexer(context.source.content));
        auto program = parser.parse();

        TypeChecker type_checker(context);
        type_checker.check(*program);

        if (context.diagnostics.has_errors()) {
            context.diagnostics.print(context.logger);
            std::exit(1);
        }

        AstDumper ast_dumper(context.types);
        ast_dumper.dump_program(*program);
        ast_dumper.dump_env(context.env);
    }
};
