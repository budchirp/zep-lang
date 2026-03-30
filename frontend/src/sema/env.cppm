module;

#include <memory>
#include <string>

export module zep.frontend.sema.env;

import zep.common.logger;
export import zep.frontend.sema.scope;
import zep.frontend.sema.builtins;

export class Env {
  private:
    Builtins builtins;

  public:
    std::unique_ptr<Scope> global_scope;
    Scope* current_scope;

    explicit Env()
        : global_scope(std::make_unique<Scope>("global", nullptr)),
          current_scope(global_scope.get()) {
        builtins.register_into(*global_scope);
    }

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
