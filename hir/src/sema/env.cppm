module;

#include <memory>
#include <string>

export module zep.hir.sema.env;

import zep.hir.sema.scope;

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
};
