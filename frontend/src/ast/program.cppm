module;

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
        Logger::print_indent(depth);

        Logger::print("Program(statements: [");
        if (statements.empty()) {
            Logger::print("])\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < statements.size(); ++i) {
                statements[i]->dump(depth + 1, true, false);
                Logger::print((i + 1 < statements.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth);
            Logger::print("])\n");
        }
    }
};
