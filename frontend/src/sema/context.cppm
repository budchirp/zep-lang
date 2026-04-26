module;

export module zep.frontend.sema.context;

import zep.common.source;
import zep.common.logger;
import zep.common.logger.diagnostic;
import zep.common.context;
import zep.frontend.sema.env;
import zep.frontend.node;
import zep.frontend.sema.type;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;

export class SemaContext {
  public:
    TypeArena types;
    ScopeArena scopes;
    SymbolArena symbols;
    NodeArena nodes;

    Env env;

    explicit SemaContext()
        : env(types, symbols, scopes) {}
};
