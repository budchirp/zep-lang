module;

#include <cstdint>
#include <string>

export module zep.frontend.sema.env;

import zep.common.span;
import zep.frontend.arena.scope;
import zep.frontend.arena.symbol;
import zep.frontend.arena.type;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;
import zep.frontend.sema.type;
import zep.frontend.sema.type.type_id;

export class Env {
  private:
    SymbolArena& symbol_arena;
    ScopeArena& scope_arena;

    void register_primitive(const std::string& primitive_name, TypeId type_id) {
        TypeSymbol* type_symbol = symbol_arena.create<TypeSymbol>(
            primitive_name, Span{}, Visibility::Type::Public, type_id);

        global_scope->define_type(primitive_name, type_symbol);
    }

  public:
    Scope* global_scope;
    Scope* current_scope;

    Env(const Env&) = delete;
    Env& operator=(const Env&) = delete;
    Env(Env&&) = delete;
    Env& operator=(Env&&) = delete;

    Env(TypeArena& type_arena, SymbolArena& symbol_arena, ScopeArena& scope_arena)
        : symbol_arena(symbol_arena), scope_arena(scope_arena) {
        global_scope = scope_arena.create("global", nullptr);
        current_scope = global_scope;

        register_primitive("void", type_arena.create<VoidType>());
        register_primitive("string", type_arena.create<StringType>());
        register_primitive("bool", type_arena.create<BooleanType>());
        register_primitive("any", type_arena.create<AnyType>());

        for (std::uint8_t size : {static_cast<std::uint8_t>(8), static_cast<std::uint8_t>(16),
                                  static_cast<std::uint8_t>(32), static_cast<std::uint8_t>(64)}) {
            register_primitive("i" + std::to_string(size),
                               type_arena.create<IntegerType>(false, size));
            register_primitive("u" + std::to_string(size),
                               type_arena.create<IntegerType>(true, size));
        }

        for (std::uint8_t size : {static_cast<std::uint8_t>(32), static_cast<std::uint8_t>(64)}) {
            register_primitive("f" + std::to_string(size), type_arena.create<FloatType>(size));
        }
    }

    void push_scope(const std::string& scope_name) {
        current_scope = scope_arena.create(scope_name, current_scope);
    }

    void push_block_scope() {
        std::string name =
            current_scope->name + ".block." + std::to_string(current_scope->next_block_index());
        current_scope = scope_arena.create(std::move(name), current_scope);
    }

    void pop_scope() {
        if (current_scope->parent != nullptr) {
            current_scope = current_scope->parent;
        }
    }
};
