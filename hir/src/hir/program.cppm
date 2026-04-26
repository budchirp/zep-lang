module;

#include <vector>

export module zep.hir.node.program;

import zep.hir.node;
import zep.hir.context;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;

export class HIRProgram {
  public:
    HIRContext context;

    Scope* global_scope;

    std::vector<HIRStatement*> statements;
    std::vector<HIRFunctionDeclaration*> functions;

    explicit HIRProgram()
        : global_scope(
              context.scope_arena.create<Scope>(Scope::Kind::Type::Global, "global", nullptr)) {}
};
