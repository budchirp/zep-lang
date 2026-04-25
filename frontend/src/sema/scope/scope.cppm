module;

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.frontend.sema.scope;

import zep.frontend.sema.kind;
import zep.frontend.sema.symbol;
import zep.frontend.sema.type;

export class Scope {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t { Global, Module, Function, Block, Struct };

        static std::string to_string(Type kind) {
            switch (kind) {
            case Type::Global:
                return "global";
            case Type::Module:
                return "module";
            case Type::Function:
                return "function";
            case Type::Block:
                return "block";
            case Type::Struct:
                return "struct";
            }
        }
    };

    const Kind::Type kind;
    const std::string name;

    std::uint64_t index = 0;

    std::unordered_map<std::string, TypeSymbol*> types;
    std::unordered_map<std::string, VarSymbol*> variables;
    std::unordered_map<std::string, std::vector<FunctionSymbol*>> functions;

    Scope* const parent;

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

    Scope(Kind::Type kind, std::string name, Scope* parent)
        : kind(kind), name(std::move(name)), parent(parent) {}

    bool define_type(const std::string& key, TypeSymbol* symbol) {
        if (types.contains(key)) {
            return false;
        }

        types[key] = symbol;
        return true;
    }

    bool define_var(const std::string& key, VarSymbol* symbol) {
        if (variables.contains(key)) {
            return false;
        }

        variables[key] = symbol;
        return true;
    }

    void define_function(const std::string& key, FunctionSymbol* symbol) {
        functions[key].push_back(symbol);
    }

    const TypeSymbol* lookup_type(const std::string& key) const {
        auto iterator = types.find(key);
        if (iterator != types.end()) {
            return iterator->second;
        }

        if (parent != nullptr) {
            return parent->lookup_type(key);
        }

        return nullptr;
    }

    const VarSymbol* lookup_var(const std::string& key) const {
        auto iterator = variables.find(key);
        if (iterator != variables.end()) {
            return iterator->second;
        }

        if (parent != nullptr) {
            return parent->lookup_var(key);
        }

        return nullptr;
    }

    const std::vector<FunctionSymbol*>& lookup_function(const std::string& key) const {
        static const std::vector<FunctionSymbol*> symbols;

        const Scope* scope = this;
        while (scope != nullptr) {
            auto iterator = scope->functions.find(key);
            if (iterator != scope->functions.end()) {
                return iterator->second;
            }

            scope = scope->parent;
        }

        return symbols;
    }

    bool has_type(const std::string& key) const {
        return types.contains(key);
    }

    bool has_variable(const std::string& key) const {
        return variables.contains(key);
    }

    bool has_function(const std::string& key) const {
        return functions.contains(key);
    }

    bool is_global() const { return parent == nullptr; }
};
