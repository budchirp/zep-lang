module;

#include <vector>

export module zep.hir.program;

import zep.hir.node;
import zep.frontend.sema.symbols;

export class HIRProgram {
  public:
    HIRNodeArena nodes;

    std::vector<const TypeSymbol*> referenced_types;

    std::vector<HIRStatement*> statements;

    HIRProgram() = default;
    HIRProgram(const HIRProgram&) = delete;
    HIRProgram& operator=(const HIRProgram&) = delete;
    HIRProgram(HIRProgram&&) = delete;
    HIRProgram& operator=(HIRProgram&&) = delete;
};
