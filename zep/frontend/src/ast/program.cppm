module;

#include <vector>

export module zep.frontend.node.program;

import zep.frontend.node;

export class Program {
  public:
    std::vector<Statement*> statements;
};
