module;

#include <string>
#include <unordered_map>
#include <utility>

export module zep.frontend.sema.scopes.environment;

import zep.common.arena;
import zep.common.source.span;
import zep.frontend.kind;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.frontend.sema.types;

export class SymbolEnvironment {
  public:
    ScopeArena scopes;

    SymbolArena symbols;

    Scope* current_scope;

    Scope* const root_scope;

    SymbolEnvironment(const SymbolEnvironment&) = delete;
    SymbolEnvironment& operator=(const SymbolEnvironment&) = delete;
    SymbolEnvironment(SymbolEnvironment&&) = delete;
    SymbolEnvironment& operator=(SymbolEnvironment&&) = delete;

    explicit SymbolEnvironment(const std::unordered_map<std::string, const Type*>& primitives)
        : current_scope(scopes.create<Scope>(Scope::Kind::Type::Global, "global", nullptr)),
          root_scope(current_scope) {
        for (const auto& [name, type] : primitives) {
            auto* symbol = symbols.create<TypeSymbol>(name, Span{}, Visibility::Type::Public, type);
            current_scope->define_type(name, symbol);
        }
    }
};
