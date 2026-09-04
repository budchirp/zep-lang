module;

#include <string>
#include <unordered_map>
#include <utility>

export module zep.frontend.sema.env;

import zep.common.arena;
import zep.common.source.span;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;

export class Env {
  public:
    ScopeArena scopes;

    SymbolArena symbols;

    Arena<OverloadSet> overloads;

    Scope* current_scope;

    Scope* const root_scope;

    Env(const Env&) = delete;
    Env& operator=(const Env&) = delete;
    Env(Env&&) = delete;
    Env& operator=(Env&&) = delete;

    explicit Env(const std::unordered_map<std::string, const Type*>& primitives)
        : current_scope(scopes.create<Scope>(Scope::Kind::Type::Global, "global", nullptr)),
          root_scope(current_scope) {

        for (const auto& [name, type] : primitives) {
            auto* symbol = symbols.create<TypeSymbol>(name, Span{}, Visibility::Type::Public, type);
            current_scope->define_type(name, symbol);
        }
    }

    void push_scope(Scope::Kind::Type kind, std::string name) {
        current_scope = scopes.create<Scope>(kind, std::move(name), current_scope);
    }

    void pop_scope() {
        if (current_scope->parent != nullptr) {
            current_scope = current_scope->parent;
        }
    }
};
