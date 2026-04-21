module;

export module zep.frontend.sema.context;

import zep.common.source;
import zep.common.logger;
import zep.common.logger.diagnostic;
import zep.frontend.arena.type;
import zep.frontend.arena.symbol;
import zep.frontend.arena.scope;
import zep.frontend.sema.env;
import zep.frontend.sema.type.type_helper;

export class Context {
  private:
  public:
    const Source& source;
    Logger logger;

    TypeArena types;
    SymbolArena symbols;
    ScopeArena scopes;

    TypeHelper type_helper;

    Env env;

    DiagnosticList diagnostics;

    explicit Context(const Source& source)
        : source(source), logger(source), type_helper(types), env(types, symbols, scopes) {}
};
