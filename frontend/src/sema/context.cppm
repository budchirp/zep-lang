module;

export module zep.frontend.sema.context;

import zep.common.source;
import zep.common.logger;
import zep.common.logger.diagnostic;
import zep.frontend.sema.env;
import zep.frontend.ast;
import zep.frontend.sema.type;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;

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
