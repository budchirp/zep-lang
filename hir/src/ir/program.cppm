module;

#include <vector>

export module zep.hir.ir.program;

import zep.hir.ir;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;

export class HIRProgram {
  public:
    HIRArena arena;
    SymbolArena symbol_arena;
    ScopeArena scope_arena;
    Scope* global_scope;
    std::vector<HIRFunctionDeclaration*> functions;

    explicit HIRProgram() : global_scope(scope_arena.create<Scope>(Scope::Kind::Type::Global, "global", nullptr)) {}
};
