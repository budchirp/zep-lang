module;

#include <vector>

export module zep.frontend.ast.program;

import zep.frontend.ast;
import zep.frontend.arena.node;

export class Program {
  private:

  public:
    NodeArena nodes;

    std::vector<Statement*> statements;
};
