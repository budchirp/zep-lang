module;

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.sema.scope;

import zep.common.logger;
import zep.sema.type;
import zep.sema.symbol;

export class Scope {
  private:
    std::vector<std::unique_ptr<Scope>> children;

    std::unordered_map<std::string, std::shared_ptr<Type>> types;
    std::unordered_map<std::string, std::unique_ptr<VarSymbol>> vars;
    std::unordered_map<std::string, std::vector<std::unique_ptr<FunctionSymbol>>> functions;

  public:
    std::string name;
    Scope* parent;

    Scope(std::string scope_name, Scope* scope_parent)
        : name(std::move(scope_name)), parent(scope_parent) {}

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
        auto* new_func_type = symbol->type ? symbol->type->as<FunctionType>() : nullptr;

        for (const auto& existing : functions[symbol_name]) {
            auto* existing_func_type =
                existing->type ? existing->type->as<FunctionType>() : nullptr;

            if (new_func_type && existing_func_type) {
                if (new_func_type->parameters.size() == existing_func_type->parameters.size()) {
                    bool same = true;
                    for (std::size_t i = 0; i < new_func_type->parameters.size(); ++i) {
                        if (!Type::compatible(new_func_type->parameters[i]->type,
                                              existing_func_type->parameters[i]->type)) {
                            same = false;
                            break;
                        }
                    }
                    if (same) {
                        return false;
                    }
                }
            } else if (!new_func_type && !existing_func_type) {
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
            for (const auto& sym : iterator->second) {
                result.push_back(sym.get());
            }
        }
        if (parent != nullptr) {
            for (const auto* sym : parent->lookup_function_overloads(symbol_name)) {
                result.push_back(sym);
            }
        }
        return result;
    }

    Scope* add_child(std::string child_name) {
        auto child = std::make_unique<Scope>(std::move(child_name), this);
        Scope* child_ptr = child.get();
        children.push_back(std::move(child));
        return child_ptr;
    }

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "Scope(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "types: [";
        if (types.empty()) {
            std::cout << "],\n";
        } else {
            std::cout << "\n";
            std::size_t type_index = 0;
            for (const auto& [type_name, type] : types) {
                ParameterType binding(type_name, type);
                binding.dump(depth + 2, true, false);
                std::cout << (++type_index < types.size() ? ",\n" : "\n");
            }
            print_indent(depth + 1);
            std::cout << "],\n";
        }

        print_indent(depth + 1);
        std::cout << "vars: [";
        if (vars.empty()) {
            std::cout << "],\n";
        } else {
            std::cout << "\n";
            std::size_t var_index = 0;
            for (const auto& entry : vars) {
                entry.second->dump(depth + 2, true, false);
                std::cout << (++var_index < vars.size() ? ",\n" : "\n");
            }
            print_indent(depth + 1);
            std::cout << "],\n";
        }

        print_indent(depth + 1);
        std::cout << "functions: [";
        std::size_t function_total = 0;
        for (const auto& entry : functions) {
            function_total += entry.second.size();
        }
        if (function_total == 0) {
            std::cout << "],\n";
        } else {
            std::cout << "\n";
            std::size_t function_index = 0;
            for (const auto& entry : functions) {
                for (const auto& func : entry.second) {
                    func->dump(depth + 2, true, false);
                    std::cout << (++function_index < function_total ? ",\n" : "\n");
                }
            }
            print_indent(depth + 1);
            std::cout << "],\n";
        }

        print_indent(depth + 1);
        std::cout << "children: [";
        if (children.empty()) {
            std::cout << "]\n";
        } else {
            std::cout << "\n";
            for (std::size_t child_index = 0; child_index < children.size(); ++child_index) {
                children[child_index]->dump(depth + 2, true, false);
                std::cout << (child_index + 1 < children.size() ? ",\n" : "\n");
            }
            print_indent(depth + 1);
            std::cout << "]\n";
        }

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
};
