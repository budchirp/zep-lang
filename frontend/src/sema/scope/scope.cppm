module;

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.frontend.sema.scope;

import zep.frontend.sema.type;
import zep.frontend.sema.type.type_id;
import zep.frontend.sema.symbol;

export class Scope {
  private:
    std::uint64_t block_counter;

    std::unordered_map<std::string, TypeSymbol*> types;
    std::unordered_map<std::string, VarSymbol*> vars;
    std::unordered_map<std::string, std::vector<FunctionSymbol*>> functions;

  public:
    std::string name;
    Scope* parent;

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(Scope&&) = delete;
    Scope& operator=(Scope&&) = delete;

    Scope(std::string name, Scope* parent)
        : block_counter(0), name(std::move(name)), parent(parent) {}

    std::uint64_t next_block_index() { return block_counter++; }

    bool local_types_empty() const { return types.empty(); }

    bool local_vars_empty() const { return vars.empty(); }

    bool local_functions_empty() const { return functions.empty(); }

    std::vector<std::pair<std::string, TypeId>> list_local_types() const {
        std::vector<std::pair<std::string, TypeId>> result;
        result.reserve(types.size());

        for (const auto& entry : types) {
            result.emplace_back(entry.first, entry.second->type);
        }

        return result;
    }

    std::vector<std::pair<std::string, const VarSymbol*>> list_local_vars() const {
        std::vector<std::pair<std::string, const VarSymbol*>> result;
        result.reserve(vars.size());

        for (const auto& entry : vars) {
            result.emplace_back(entry.first, entry.second);
        }

        return result;
    }

    std::vector<std::pair<std::string, const FunctionSymbol*>> list_local_functions() const {
        std::vector<std::pair<std::string, const FunctionSymbol*>> result;

        for (const auto& entry : functions) {
            for (FunctionSymbol* symbol : entry.second) {
                result.emplace_back(entry.first, symbol);
            }
        }

        return result;
    }

    bool define_type(const std::string& key, TypeSymbol* symbol) {
        if (types.contains(key)) {
            return false;
        }

        types[key] = symbol;
        return true;
    }

    bool define_var(const std::string& key, VarSymbol* symbol) {
        if (vars.contains(key)) {
            return false;
        }

        vars[key] = symbol;
        return true;
    }

    void add_function(const std::string& key, FunctionSymbol* symbol) {
        functions[key].push_back(symbol);
    }

    bool has_local_var(const std::string& key) const { return vars.contains(key); }

    TypeId lookup_type(const std::string& key) const {
        auto iterator = types.find(key);
        if (iterator != types.end()) {
            return iterator->second->type;
        }

        if (parent != nullptr) {
            return parent->lookup_type(key);
        }

        return TypeId{};
    }

    const TypeSymbol* lookup_type_symbol(const std::string& key) const {
        auto iterator = types.find(key);
        if (iterator != types.end()) {
            return iterator->second;
        }

        if (parent != nullptr) {
            return parent->lookup_type_symbol(key);
        }

        return nullptr;
    }

    const VarSymbol* lookup_var(const std::string& key) const {
        auto iterator = vars.find(key);
        if (iterator != vars.end()) {
            return iterator->second;
        }

        if (parent != nullptr) {
            return parent->lookup_var(key);
        }

        return nullptr;
    }

    std::vector<const FunctionSymbol*> local_function_overloads(const std::string& key) const {
        auto iterator = functions.find(key);
        if (iterator == functions.end()) {
            return {};
        }

        std::vector<const FunctionSymbol*> result;
        result.reserve(iterator->second.size());

        for (FunctionSymbol* symbol : iterator->second) {
            result.push_back(symbol);
        }

        return result;
    }

    std::vector<const FunctionSymbol*> lookup_function_overloads(const std::string& key) const {
        std::vector<const FunctionSymbol*> result;

        const Scope* scope = this;
        while (scope != nullptr) {
            auto iterator = scope->functions.find(key);
            if (iterator != scope->functions.end()) {
                result.insert(result.end(), iterator->second.begin(), iterator->second.end());
            }

            scope = scope->parent;
        }

        return result;
    }
};
