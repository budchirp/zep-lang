module;

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.frontend.sema.scope;

export import :symbol;
import zep.frontend.sema.kind;
import zep.frontend.sema.type;
import zep.common.arena;

export class Scope {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t { Global, Module, Function, Block, Struct, Enum, Interface };

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
            case Type::Enum:
                return "enum";
            case Type::Interface:
                return "interface";
            }
        }
    };

  private:
    std::unordered_map<std::string, TypeSymbol*> types;
    std::unordered_map<std::string, VariableSymbol*> variables;
    std::unordered_map<std::string, OverloadSet*> functions;
    std::unordered_map<std::string, EnumVariantSymbol*> variants;

  public:
    const Kind::Type kind;
    const std::string name;
    Scope* const parent;

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

    Scope(Kind::Type kind, std::string name, Scope* parent)
        : kind(kind), name(std::move(name)), parent(parent) {}

    bool define_type(const std::string& key, TypeSymbol* symbol) {
        if (!types.emplace(key, symbol).second) {
            return false;
        }

        return true;
    }

    bool define_var(const std::string& key, VariableSymbol* symbol) {
        if (!variables.emplace(key, symbol).second) {
            return false;
        }

        return true;
    }

    void define_function(const std::string& key, FunctionSymbol* symbol,
                         OverloadSet* overload_set) {
        if (functions.contains(key)) {
            functions[key]->functions.push_back(symbol);
            return;
        }

        overload_set->functions.push_back(symbol);
        functions.emplace(key, overload_set);
    }

    bool define_variant(const std::string& key, EnumVariantSymbol* symbol) {
        if (!variants.emplace(key, symbol).second) {
            return false;
        }

        return true;
    }

    const TypeSymbol* lookup_type(const std::string& key) const {
        for (const auto* scope = this; scope != nullptr; scope = scope->parent) {
            if (auto iterator = scope->types.find(key); iterator != scope->types.end()) {
                return iterator->second;
            }
        }

        return nullptr;
    }

    const VariableSymbol* lookup_var(const std::string& key) const {
        for (const auto* scope = this; scope != nullptr; scope = scope->parent) {
            if (auto iterator = scope->variables.find(key); iterator != scope->variables.end()) {
                return iterator->second;
            }
        }

        return nullptr;
    }

    const std::vector<FunctionSymbol*>& lookup_function_overloads(const std::string& key) const {
        static const std::vector<FunctionSymbol*> empty;

        for (const auto* scope = this; scope != nullptr; scope = scope->parent) {
            if (auto iterator = scope->functions.find(key); iterator != scope->functions.end()) {
                return iterator->second->functions;
            }
        }

        return empty;
    }

    const FunctionSymbol* lookup_function(const std::string& key) const {
        const auto& overloads = lookup_function_overloads(key);
        return overloads.empty() ? nullptr : overloads[0];
    }

    const EnumVariantSymbol* lookup_variant(const std::string& key) const {
        for (const auto* scope = this; scope != nullptr; scope = scope->parent) {
            if (auto iterator = scope->variants.find(key); iterator != scope->variants.end()) {
                return iterator->second;
            }
        }

        return nullptr;
    }

    const TypeSymbol* find_local_type(const std::string& key) const {
        auto iterator = types.find(key);
        return iterator != types.end() ? iterator->second : nullptr;
    }

    const VariableSymbol* find_local_var(const std::string& key) const {
        auto iterator = variables.find(key);
        return iterator != variables.end() ? iterator->second : nullptr;
    }

    const std::vector<FunctionSymbol*>*
    find_local_function_overloads(const std::string& key) const {
        auto iterator = functions.find(key);
        return iterator != functions.end() ? &iterator->second->functions : nullptr;
    }

    const FunctionSymbol* find_local_function(const std::string& key) const {
        const auto* overloads = find_local_function_overloads(key);
        return (overloads != nullptr && !overloads->empty()) ? overloads->front() : nullptr;
    }

    const EnumVariantSymbol* find_local_variant(const std::string& key) const {
        auto iterator = variants.find(key);
        return iterator != variants.end() ? iterator->second : nullptr;
    }

    bool has_local_var(const std::string& key) const { return variables.contains(key); }

    bool has_local_type(const std::string& key) const { return types.contains(key); }

    bool has_local_function(const std::string& key) const { return functions.contains(key); }

    bool has_local_variant(const std::string& key) const { return variants.contains(key); }

    const TypeSymbol* find_exported_type(const std::string& key) const {
        if (auto iterator = types.find(key);
            iterator != types.end() && iterator->second->visibility == Visibility::Type::Public) {
            return iterator->second;
        }

        return nullptr;
    }

    const VariableSymbol* find_exported_var(const std::string& key) const {
        if (auto iterator = variables.find(key);
            iterator != variables.end() &&
            iterator->second->visibility == Visibility::Type::Public) {
            return iterator->second;
        }

        return nullptr;
    }

    std::vector<FunctionSymbol*> find_exported_function_overloads(const std::string& key) const {
        if (auto iterator = functions.find(key); iterator != functions.end()) {
            std::vector<FunctionSymbol*> result;
            result.reserve(iterator->second->functions.size());

            for (auto* symbol : iterator->second->functions) {
                if (symbol != nullptr && symbol->visibility == Visibility::Type::Public) {
                    result.push_back(symbol);
                }
            }

            return result;
        }

        return {};
    }

    const EnumVariantSymbol* find_exported_variant(const std::string& key) const {
        if (auto iterator = variants.find(key);
            iterator != variants.end() &&
            iterator->second->visibility == Visibility::Type::Public) {
            return iterator->second;
        }

        return nullptr;
    }

    const std::unordered_map<std::string, OverloadSet*>& local_functions() const {
        return functions;
    }

    const std::unordered_map<std::string, TypeSymbol*>& local_types() const { return types; }

    const std::unordered_map<std::string, VariableSymbol*>& local_variables() const {
        return variables;
    }

    const std::unordered_map<std::string, EnumVariantSymbol*>& local_variants() const {
        return variants;
    }

    std::size_t local_binding_count() const {
        std::size_t count = types.size() + variables.size() + functions.size() + variants.size();
        return count;
    }

    bool is_global() const { return parent == nullptr; }
};

export using ScopeArena = Arena<Scope>;

export class ScopeGuard {
  private:
    Scope*& current_scope;
    Scope* saved_scope;

  public:
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard(ScopeGuard&&) = delete;
    ScopeGuard& operator=(ScopeGuard&&) = delete;

    ScopeGuard(Scope*& current_scope, ScopeArena& scopes, Scope::Kind::Type kind, std::string name)
        : current_scope(current_scope), saved_scope(current_scope) {
        current_scope = scopes.create<Scope>(kind, std::move(name), current_scope);
    }

    ScopeGuard(Scope*& current_scope, Scope* scope)
        : current_scope(current_scope), saved_scope(current_scope) {
        current_scope = scope;
    }

    ~ScopeGuard() { current_scope = saved_scope; }
};
