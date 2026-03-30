module;

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.lowerer.sema.scope;

import zep.common.logger;
import zep.lowerer.sema.scope.symbol;
import zep.lowerer.sema.type;

export class LoweredScope {
  private:
    std::vector<std::unique_ptr<LoweredScope>> children;

    std::unordered_map<std::string, std::unique_ptr<LoweredTypeSymbol>> types;
    std::unordered_map<std::string, std::unique_ptr<LoweredVarSymbol>> vars;
    std::unordered_map<std::string, std::unique_ptr<LoweredFunctionSymbol>> functions;

  public:
    std::string name;
    LoweredScope* parent;

    LoweredScope(std::string name, LoweredScope* parent) : name(std::move(name)), parent(parent) {}

    std::size_t child_count() const { return children.size(); }

    bool define_type(const std::string& symbol_name, std::unique_ptr<LoweredTypeSymbol> symbol) {
        if (types.contains(symbol_name)) {
            return false;
        }
        types[symbol_name] = std::move(symbol);
        return true;
    }

    bool define_var(const std::string& symbol_name, std::unique_ptr<LoweredVarSymbol> symbol) {
        if (vars.contains(symbol_name)) {
            return false;
        }
        vars[symbol_name] = std::move(symbol);
        return true;
    }

    bool define_function(const std::string& symbol_name,
                         std::unique_ptr<LoweredFunctionSymbol> symbol) {
        if (functions.contains(symbol_name)) {
            return false;
        }
        functions[symbol_name] = std::move(symbol);
        return true;
    }

    const LoweredTypeSymbol* lookup_type(const std::string& symbol_name) const {
        auto iterator = types.find(symbol_name);
        if (iterator != types.end()) {
            return iterator->second.get();
        }
        if (parent != nullptr) {
            return parent->lookup_type(symbol_name);
        }
        return nullptr;
    }

    const LoweredVarSymbol* lookup_var(const std::string& symbol_name) const {
        auto iterator = vars.find(symbol_name);
        if (iterator != vars.end()) {
            return iterator->second.get();
        }
        if (parent != nullptr) {
            return parent->lookup_var(symbol_name);
        }
        return nullptr;
    }

    const LoweredFunctionSymbol* lookup_function(const std::string& symbol_name) const {
        auto iterator = functions.find(symbol_name);
        if (iterator != functions.end()) {
            return iterator->second.get();
        }
        if (parent != nullptr) {
            return parent->lookup_function(symbol_name);
        }
        return nullptr;
    }

    std::shared_ptr<LoweredType> lookup_type_as_lowered(const std::string& symbol_name) const {
        const auto* symbol = lookup_type(symbol_name);
        return symbol != nullptr ? symbol->type : nullptr;
    }

    LoweredScope* add_child(std::string child_name) {
        auto child = std::make_unique<LoweredScope>(std::move(child_name), this);
        LoweredScope* child_ptr = child.get();
        children.push_back(std::move(child));
        return child_ptr;
    }

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("Scope(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("types: [");
        if (types.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            std::size_t type_index = 0;
            for (const auto& entry : types) {
                entry.second->dump(depth + 2, true, false);
                Logger::print((++type_index < types.size() ? ",\n" : "\n"));
            }
            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("vars: [");
        if (vars.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            std::size_t var_index = 0;
            for (const auto& entry : vars) {
                entry.second->dump(depth + 2, true, false);
                Logger::print((++var_index < vars.size() ? ",\n" : "\n"));
            }
            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("functions: [");
        if (functions.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            std::size_t function_index = 0;
            for (const auto& entry : functions) {
                entry.second->dump(depth + 2, true, false);
                Logger::print((++function_index < functions.size() ? ",\n" : "\n"));
            }
            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("children: [");
        if (children.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (std::size_t child_index = 0; child_index < children.size(); ++child_index) {
                children[child_index]->dump(depth + 2, true, false);
                Logger::print((child_index + 1 < children.size() ? ",\n" : "\n"));
            }
            Logger::print_indent(depth + 1);
            Logger::print("]\n");
        }

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};
