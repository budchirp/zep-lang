module;

#include <cstdint>
#include <string>
#include <utility>
#include <unordered_map>

export module zep.frontend.sema.env;

import zep.common.span;
import zep.frontend.arena;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;
import zep.frontend.sema.type;

export class Env {
  private:
    ScopeArena& scope_arena;

    void init(TypeArena& type_arena) {
        primitives["void"] = type_arena.create<VoidType>();
        primitives["string"] = type_arena.create<StringType>();
        primitives["boolean"] = type_arena.create<BooleanType>();
        primitives["any"] = type_arena.create<AnyType>();

        for (std::uint8_t size : {static_cast<std::uint8_t>(8), static_cast<std::uint8_t>(16),
                                  static_cast<std::uint8_t>(32), static_cast<std::uint8_t>(64)}) {
            primitives["i" + std::to_string(size)] = type_arena.create<IntegerType>(false, size);
            primitives["u" + std::to_string(size)] = type_arena.create<IntegerType>(true, size);
        }

        for (std::uint8_t size : {static_cast<std::uint8_t>(32), static_cast<std::uint8_t>(64)}) {
            primitives["f" + std::to_string(size)] = type_arena.create<FloatType>(size);
        }
    }

  public:
    Scope* current_scope;
    Scope* const global_scope;

    std::unordered_map<std::string, const Type*> primitives;

    Env(const Env&) = delete;
    Env& operator=(const Env&) = delete;
    Env(Env&&) = delete;
    Env& operator=(Env&&) = delete;

    Env(TypeArena& type_arena, SymbolArena& symbol_arena, ScopeArena& scope_arena)
        : scope_arena(scope_arena),
          current_scope(scope_arena.create<Scope>(Scope::Kind::Type::Global, "global", nullptr)),
          global_scope(current_scope) {
        init(type_arena);

        for (const auto& [name, type] : primitives) {
            auto* symbol = symbol_arena.create<TypeSymbol>(name, Span{}, Visibility::Type::Public, type);
            current_scope->define_type(name, symbol);
        }
    }

    void push_scope(Scope::Kind::Type kind, std::string name) {
        current_scope = scope_arena.create<Scope>(kind, std::move(name), current_scope);
    }

    void pop_scope() {
        if (current_scope->parent != nullptr) {
            current_scope = current_scope->parent;
        }
    }
};
