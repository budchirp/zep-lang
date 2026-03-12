module;

#include <iostream>
#include <memory>
#include <vector>

export module zep.frontend.ast.program;

import zep.frontend.ast;

export class Program {
  public:
    std::vector<std::unique_ptr<Statement>> statements;

    explicit Program(std::vector<std::unique_ptr<Statement>> statements)
        : statements(std::move(statements)) {}

    void dump() const {
        std::cout << "Program {\n";
        std::cout << "  statements: [\n";
        for (const auto& statement : statements) {
            statement->dump(2);
        }
        std::cout << "  ]\n";
        std::cout << "}\n";
    }
};
