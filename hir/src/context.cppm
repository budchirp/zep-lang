module;

export module zep.hir.context;

import zep.common.context;
import zep.hir.node;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;

export class HIRContext {
  public:
    HIRNodeArena nodes;

    SymbolArena symbol_arena;
    ScopeArena scope_arena;

    HIRContext() = default;
};
