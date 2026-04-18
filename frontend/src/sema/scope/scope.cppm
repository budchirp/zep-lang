module;

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.frontend.sema.scope;

import zep.frontend.sema.type;
import zep.frontend.sema.symbol;

export class Scope {
  public:
    std::vector<std::unique_ptr<Scope>> children;

    std::unordered_map<std::string, std::shared_ptr<Type>> types;
    std::unordered_map<std::string, std::unique_ptr<VarSymbol>> vars;
    std::unordered_map<std::string, std::vector<std::unique_ptr<FunctionSymbol>>> functions;

    std::string name;
    Scope* parent;

    Scope(std::string name, Scope* parent)
        : name(std::move(name)), parent(parent) {}

    [[nodiscard]] std::size_t child_count() const { return children.size(); }

    bool define_type(const std::string& symbol_name, std::shared_ptr<Type> type) {
        if (types.contains(symbol_name)) {
            return false;
        }

        types[symbol_name] = std::move(type);
        return true;
    }

    bool define_var(const std::string& symbol_name, std::unique_ptr<VarSymbol> symbol) {
        if (vars.contains(symbol_name)) {
            return false;
        }

        vars[symbol_name] = std::move(symbol);
        return true;
    }

    bool define_function(const std::string& symbol_name, std::unique_ptr<FunctionSymbol> symbol) {
        auto* new_type = symbol->type != nullptr ? symbol->type->as<FunctionType>() : nullptr;

        for (const auto& existing : functions[symbol_name]) {
            auto* existing_type =
                existing->type != nullptr ? existing->type->as<FunctionType>() : nullptr;

            if (new_type != nullptr && new_type->conflicts_with(existing_type)) {
                return false;
            }
        }

        functions[symbol_name].push_back(std::move(symbol));
        return true;
    }

    std::shared_ptr<Type> lookup_type(const std::string& symbol_name) const {
        auto iterator = types.find(symbol_name);
        if (iterator != types.end()) {
            return iterator->second;
        }

        if (parent != nullptr) {
            return parent->lookup_type(symbol_name);
        }

        return nullptr;
    }

    const VarSymbol* lookup_var(const std::string& symbol_name) const {
        auto iterator = vars.find(symbol_name);
        if (iterator != vars.end()) {
            return iterator->second.get();
        }

        if (parent != nullptr) {
            return parent->lookup_var(symbol_name);
        }

        return nullptr;
    }

    const FunctionSymbol* lookup_function(const std::string& symbol_name) const {
        auto iterator = functions.find(symbol_name);
        if (iterator != functions.end() && !iterator->second.empty()) {
            return iterator->second.front().get();
        }

        if (parent != nullptr) {
            return parent->lookup_function(symbol_name);
        }

        return nullptr;
    }

    std::vector<const FunctionSymbol*>
    lookup_function_overloads(const std::string& symbol_name) const {
        std::vector<const FunctionSymbol*> result;

        auto iterator = functions.find(symbol_name);
        if (iterator != functions.end()) {
            result.reserve(iterator->second.size());
            for (const auto& symbol : iterator->second) {
                result.push_back(symbol.get());
            }
        }

        if (parent != nullptr) {
            auto parent_overloads = parent->lookup_function_overloads(symbol_name);
            result.insert(result.end(), parent_overloads.begin(), parent_overloads.end());
        }

        return result;
    }

    const FunctionSymbol* resolve_overload(const std::string& symbol_name,
                                           const std::function<bool(const FunctionType*)>& matcher,
                                           int& match_count) const {
        auto overloads = lookup_function_overloads(symbol_name);
        const FunctionSymbol* best_match = nullptr;
        match_count = 0;

        for (const auto* symbol : overloads) {
            auto* candidate_type =
                symbol->type != nullptr ? symbol->type->as<FunctionType>() : nullptr;
            if (candidate_type == nullptr) {
                continue;
            }

            if (matcher(candidate_type)) {
                best_match = symbol;
                ++match_count;
            }
        }

        return match_count == 1 ? best_match : nullptr;
    }

    Scope* add_child(std::string child_name) {
        auto child = std::make_unique<Scope>(std::move(child_name), this);
        Scope* child_ptr = child.get();
        children.push_back(std::move(child));
        return child_ptr;
    }
};
