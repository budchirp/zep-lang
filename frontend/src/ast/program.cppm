module;

#include <memory>
#include <vector>

export module zep.frontend.ast.program;

import zep.frontend.ast;

export class Program {
  public:
    std::vector<std::unique_ptr<Statement>> statements;

    explicit Program(std::vector<std::unique_ptr<Statement>> statements)
        : statements(std::move(statements)) {}
};
