module;

#include <memory>
#include <string>

export module zep.hir.sema.env;

import zep.common.logger;
export import zep.hir.sema.scope;

export class HIREnv {
  public:
    std::unique_ptr<HIRScope> global_scope;
    HIRScope* current_scope;

    explicit HIREnv()
        : global_scope(std::make_unique<HIRScope>("global", nullptr)),
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
        Logger::print_indent(depth);
        Logger::print("Env(\n");

        Logger::print_indent(depth + 1);
        Logger::print("global_scope: ");
        global_scope->dump(depth + 1, false, true);

        Logger::print_indent(depth);
        Logger::print(")\n");
    }
};
