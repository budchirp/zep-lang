module;

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.hir.sema.scope;

import zep.hir.sema.scope.symbol;
import zep.hir.sema.type;

export class HIRScope {
  public:
    std::vector<std::unique_ptr<HIRScope>> children;

    std::unordered_map<std::string, std::unique_ptr<HIRTypeSymbol>> types;
    std::unordered_map<std::string, std::unique_ptr<HIRVarSymbol>> vars;
    std::unordered_map<std::string, std::unique_ptr<HIRFunctionSymbol>> functions;

    std::string name;
    HIRScope* parent;

    HIRScope(std::string name, HIRScope* parent) : name(std::move(name)), parent(parent) {}

    std::size_t child_count() const { return children.size(); }

    bool define_type(const std::string& symbol_name, std::unique_ptr<HIRTypeSymbol> symbol) {
        if (types.contains(symbol_name)) {
            return false;
        }
        types[symbol_name] = std::move(symbol);
        return true;
    }

    bool define_var(const std::string& symbol_name, std::unique_ptr<HIRVarSymbol> symbol) {
        if (vars.contains(symbol_name)) {
            return false;
        }
        vars[symbol_name] = std::move(symbol);
        return true;
    }

    bool define_function(const std::string& symbol_name,
                         std::unique_ptr<HIRFunctionSymbol> symbol) {
        if (functions.contains(symbol_name)) {
            return false;
        }
        functions[symbol_name] = std::move(symbol);
        return true;
    }

    const HIRTypeSymbol* lookup_type(const std::string& symbol_name) const {
        auto iterator = types.find(symbol_name);
        if (iterator != types.end()) {
            return iterator->second.get();
        }
        if (parent != nullptr) {
            return parent->lookup_type(symbol_name);
        }
        return nullptr;
    }

    const HIRVarSymbol* lookup_var(const std::string& symbol_name) const {
        auto iterator = vars.find(symbol_name);
        if (iterator != vars.end()) {
            return iterator->second.get();
        }
        if (parent != nullptr) {
            return parent->lookup_var(symbol_name);
        }
        return nullptr;
    }

    const HIRFunctionSymbol* lookup_function(const std::string& symbol_name) const {
        auto iterator = functions.find(symbol_name);
        if (iterator != functions.end()) {
            return iterator->second.get();
        }
        if (parent != nullptr) {
            return parent->lookup_function(symbol_name);
        }
        return nullptr;
    }

    std::shared_ptr<HIRType> lookup_type_as_hir(const std::string& symbol_name) const {
        const auto* symbol = lookup_type(symbol_name);
        return symbol != nullptr ? symbol->type : nullptr;
    }

    HIRScope* add_child(std::string child_name) {
        auto child = std::make_unique<HIRScope>(std::move(child_name), this);
        HIRScope* child_ptr = child.get();
        children.push_back(std::move(child));
        return child_ptr;
    }
};
