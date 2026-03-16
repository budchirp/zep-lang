module;

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

export module zep.lowerer.monomorphizer;

import zep.sema.type;
import zep.frontend.ast;
import zep.frontend.ast.program;
import zep.lowerer.name_mangler;

export class Monomorphizer {
  public:
    struct PendingSpecialization {
        enum class Kind { Function, Struct };

        Kind kind;
        std::string original_name;
        std::string mangled_name;
        std::vector<std::shared_ptr<Type>> type_arguments;
    };

  private:
    std::unordered_map<std::string, FunctionDeclaration*> generic_functions;
    std::unordered_map<std::string, StructDeclaration*> generic_structs;
    std::unordered_set<std::string> extern_function_names;
    std::unordered_set<std::string> extern_var_names;
    std::unordered_set<std::string> emitted_functions;
    std::unordered_set<std::string> emitted_structs;
    std::vector<PendingSpecialization> worklist;

  public:
    void index_declarations(Program& program) {
        for (auto& statement : program.statements) {
            if (auto* function = statement->as<FunctionDeclaration>()) {
                if (!function->prototype->generic_parameters.empty()) {
                    generic_functions[function->prototype->name] = function;
                }
            } else if (auto* extern_function = statement->as<ExternFunctionDeclaration>()) {
                extern_function_names.insert(extern_function->prototype->name);
            } else if (auto* struct_declaration = statement->as<StructDeclaration>()) {
                if (!struct_declaration->generic_parameters.empty()) {
                    generic_structs[struct_declaration->name] = struct_declaration;
                }
            } else if (auto* extern_var = statement->as<ExternVarDeclaration>()) {
                extern_var_names.insert(extern_var->name);
            }
        }
    }

    std::string request_function_specialization(
        const std::string& name,
        const std::vector<std::shared_ptr<Type>>& type_arguments,
        const std::vector<std::shared_ptr<Type>>& resolved_parameter_types,
        const std::shared_ptr<Type>& resolved_return_type,
        bool is_variadic) {
        auto mangled_name = NameMangler::mangle_function(name, type_arguments,
                                                         resolved_parameter_types,
                                                         resolved_return_type, is_variadic);

        if (emitted_functions.find(mangled_name) == emitted_functions.end()) {
            emitted_functions.insert(mangled_name);
            worklist.push_back(PendingSpecialization{PendingSpecialization::Kind::Function, name,
                                                     mangled_name, type_arguments});
        }

        return mangled_name;
    }

    std::string request_struct_specialization(
        const std::string& name,
        const std::vector<std::shared_ptr<Type>>& type_arguments) {
        auto mangled_name = NameMangler::mangle_struct(name, type_arguments);

        if (emitted_structs.find(mangled_name) == emitted_structs.end()) {
            emitted_structs.insert(mangled_name);
            worklist.push_back(PendingSpecialization{PendingSpecialization::Kind::Struct, name,
                                                     mangled_name, type_arguments});
        }

        return mangled_name;
    }

    bool is_generic_function(const std::string& name) const {
        return generic_functions.find(name) != generic_functions.end();
    }

    bool is_generic_struct(const std::string& name) const {
        return generic_structs.find(name) != generic_structs.end();
    }

    bool is_extern_function(const std::string& name) const {
        return extern_function_names.find(name) != extern_function_names.end();
    }

    bool is_extern_var(const std::string& name) const {
        return extern_var_names.find(name) != extern_var_names.end();
    }

    FunctionDeclaration* get_function_declaration(const std::string& name) const {
        auto iterator = generic_functions.find(name);
        return iterator != generic_functions.end() ? iterator->second : nullptr;
    }

    StructDeclaration* get_struct_declaration(const std::string& name) const {
        auto iterator = generic_structs.find(name);
        return iterator != generic_structs.end() ? iterator->second : nullptr;
    }

    bool has_pending() const { return !worklist.empty(); }

    PendingSpecialization pop_pending() {
        auto pending = std::move(worklist.back());
        worklist.pop_back();
        return pending;
    }
};
