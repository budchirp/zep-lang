module;

export module zep.frontend.sema.context;

import zep.common.source;
import zep.common.logger;
import zep.common.logger.diagnostic;
import zep.frontend.arena;
import zep.frontend.arena;
import zep.frontend.sema.env;

export class Context {
  public:
    const Source& source;
    Logger logger;

    TypeArena types;
    ScopeArena scopes;
    SymbolArena symbols;
    NodeArena nodes;

    Env env;

    DiagnosticList diagnostics;

    explicit Context(const Source& source)
        : source(source), logger(source), env(types, symbols, scopes) {}
};
