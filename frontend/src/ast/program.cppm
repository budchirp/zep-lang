module;

#include <iostream>
#include <memory>
#include <vector>

export module zep.frontend.ast.program;

import zep.common.logger;
import zep.frontend.ast;

export class Program {
  public:
    std::vector<std::unique_ptr<Statement>> statements;

    explicit Program(std::vector<std::unique_ptr<Statement>> statements)
        : statements(std::move(statements)) {}

    void dump() const {
        constexpr int depth = 0;
        print_indent(depth);

        std::cout << "Program(statements: [";
        if (statements.empty()) {
            std::cout << "])\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < statements.size(); ++i) {
                statements[i]->dump(depth + 1, true, false);
                std::cout << (i + 1 < statements.size() ? ",\n" : "\n");
            }

            print_indent(depth);
            std::cout << "])\n";
        }
    }
};
