module;

#include <memory>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.sema.env;

import zep.sema.type;
import zep.sema.symbol;
import zep.sema.builtins;

export class Scope {
  private:
    std::unordered_map<std::string, std::unique_ptr<Symbol>> symbols;

  public:
    Scope() = default;

    bool define(const std::string& name, std::unique_ptr<Symbol> symbol) {
        auto [_, inserted] = symbols.try_emplace(name, std::move(symbol));
        return inserted;
    }

    const Symbol* lookup(const std::string& name) const {
        auto iterator = symbols.find(name);
        if (iterator != symbols.end()) {
            return iterator->second.get();
        }

        return nullptr;
    }
};

export class Env {
  private:
    Builtins builtins;

    std::vector<Scope> scopes;
    std::unordered_map<std::string, std::shared_ptr<Type>> types;

  public:
    explicit Env() {}

    void push_scope() { scopes.emplace_back(); }

    void pop_scope() { scopes.pop_back(); }

    bool define(const std::string& name, std::unique_ptr<Symbol> symbol) {
        return scopes.back().define(name, std::move(symbol));
    }

    const Symbol* lookup(const std::string& name) const {
        for (const auto& scope : std::views::reverse(scopes)) {
            const Symbol* result = scope.lookup(name);
            if (result != nullptr) {
                return result;
            }
        }

        return nullptr;
    }

    void define_type(const std::string& name, const std::shared_ptr<Type>& type) {
        types.try_emplace(name, type);
    }

    std::shared_ptr<Type> lookup_type(const std::string& name) const {
        auto builtin = builtins.lookup_type(name);
        if (builtin) {
            return builtin;
        }
        auto iterator = types.find(name);
        if (iterator != types.end()) {
            return iterator->second;
        }
        return nullptr;
    }
};
