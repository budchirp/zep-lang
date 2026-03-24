module;

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.lowerer.sema.scope;

import zep.common.logger;
import zep.lowerer.sema.scope.symbol;
import zep.lowerer.sema.types;

export class LoweredScope {
  private:
    std::vector<std::unique_ptr<LoweredScope>> children;

    std::unordered_map<std::string, std::unique_ptr<LoweredTypeSymbol>> types;
    std::unordered_map<std::string, std::unique_ptr<LoweredVarSymbol>> vars;
    std::unordered_map<std::string, std::vector<std::unique_ptr<LoweredFunctionSymbol>>> functions;

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
        auto& bucket = functions[symbol_name];
        for (const auto& existing : bucket) {
            if (existing->type != nullptr && symbol->type != nullptr &&
                existing->type->kind == LoweredType::Kind::Function &&
                symbol->type->kind == LoweredType::Kind::Function) {
                auto* existing_fn = existing->type->as<LoweredFunctionType>();
                auto* new_fn = symbol->type->as<LoweredFunctionType>();
                if (existing_fn != nullptr && new_fn != nullptr &&
                    existing_fn->parameters.size() == new_fn->parameters.size()) {
                    bool same = true;
                    for (std::size_t i = 0; i < existing_fn->parameters.size() && same; ++i) {
                        auto a = existing_fn->parameters[i] != nullptr
                                     ? existing_fn->parameters[i]->to_string()
                                     : "";
                        auto b = new_fn->parameters[i] != nullptr
                                     ? new_fn->parameters[i]->to_string()
                                     : "";
                        if (a != b) {
                            same = false;
                        }
                    }
                    if (same) {
                        return false;
                    }
                }
            }
        }
        bucket.push_back(std::move(symbol));
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
        if (iterator != functions.end() && !iterator->second.empty()) {
            return iterator->second.front().get();
        }
        if (parent != nullptr) {
            return parent->lookup_function(symbol_name);
        }
        return nullptr;
    }

    std::vector<const LoweredFunctionSymbol*>
    lookup_function_overloads(const std::string& symbol_name) const {
        std::vector<const LoweredFunctionSymbol*> result;
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
            for (const auto& entry : types) {
                entry.second->dump(depth + 2, true, false);
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
                for (const auto& sym : entry.second) {
                    sym->dump(depth + 2, true, false);
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
