module;

#include <iostream>
#include <memory>
#include <string>

export module zep.lowerer.sema.env;

import zep.common.logger;
export import zep.lowerer.sema.scope;

export class LoweredEnv {
  public:
    std::unique_ptr<LoweredScope> global_scope;
    LoweredScope* current_scope;

    explicit LoweredEnv()
        : global_scope(std::make_unique<LoweredScope>("global", nullptr)),
          current_scope(global_scope.get()) {}

    void push_scope(const std::string& scope_name) {
        current_scope = current_scope->add_child(scope_name);
    }

    void pop_scope() {
        if (current_scope->parent != nullptr) {
            current_scope = current_scope->parent;
        }
    }

    void dump(int depth = 0) const {
        print_indent(depth);
        std::cout << "Env(\n";

        print_indent(depth + 1);
        std::cout << "global_scope: ";
        global_scope->dump(depth + 1, false, true);

        print_indent(depth);
        std::cout << ")\n";
    }
};
