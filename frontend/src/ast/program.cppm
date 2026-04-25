module;

#include <vector>

export module zep.frontend.ast.program;

import zep.frontend.ast;
import zep.frontend.arena;

export class Program {
  public:
    NodeArena nodes;

    std::vector<Statement*> statements;
};
