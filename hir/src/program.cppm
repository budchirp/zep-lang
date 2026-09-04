module;

#include <vector>

export module zep.hir.program;

import zep.hir.node;
import zep.hir.context;
import zep.frontend.sema.scope;

export class HIRProgram {
  public:
    HIRContext context;

    std::vector<const TypeSymbol*> referenced_types;
    std::vector<HIRVarDeclaration*> globals;
    std::vector<HIRFunctionDeclaration*> functions;

    std::vector<HIRStatement*> statements;

    HIRProgram() = default;
    HIRProgram(const HIRProgram&) = delete;
    HIRProgram& operator=(const HIRProgram&) = delete;
    HIRProgram(HIRProgram&&) = delete;
    HIRProgram& operator=(HIRProgram&&) = delete;
};
