module;

#include <vector>

export module zep.frontend.ast.program;

import zep.frontend.ast;

export class Program {
  public:
    std::vector<Statement*> statements;

    Program() = default;
};
