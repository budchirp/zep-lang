module;

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.checker.type_context;

import zep.sema.type;
import zep.sema.env;

export class TypeContext {
  private:
    Env& env;

    std::unordered_map<std::string, std::shared_ptr<Type>> generic_substitutions;

    std::shared_ptr<Type> resolve_type_impl(std::shared_ptr<Type> type) const {
        if (!type) {
            return nullptr;
        }

        switch (type->kind) {

        case Type::Kind::Named: {
            auto* named = type->as<NamedType>();
            if (named == nullptr) {
                return type;
            }

            auto base = resolve_type_name(named->name);
            if (base == nullptr) {
                return type;
            }

            if (!named->generic_arguments.empty()) {
                auto* struct_type = base->as<StructType>();
                if (struct_type != nullptr &&
                    named->generic_arguments.size() == struct_type->generic_parameters.size()) {
                    std::unordered_map<std::string, std::shared_ptr<Type>> substitution_map;
                    for (std::size_t i = 0; i < named->generic_arguments.size(); ++i) {
                        auto arg_resolved = resolve_type_impl(named->generic_arguments[i]->type);
                        substitution_map[struct_type->generic_parameters[i].name] = arg_resolved;
                    }
                    auto substituted = substitute_generic_type(base, substitution_map);
                    return resolve_type_impl(substituted);
                }
            }

            return resolve_type_impl(base);
        }

        case Type::Kind::Pointer: {
            auto* pointer = type->as<PointerType>();
            if (pointer == nullptr) {
                return type;
            }

            auto resolved_element = resolve_type_impl(pointer->element);
            return std::make_shared<PointerType>(
                resolved_element ? resolved_element : pointer->element, pointer->is_mutable);
        }

        case Type::Kind::Array: {
            auto* array = type->as<ArrayType>();
            if (array == nullptr) {
                return type;
            }

            auto resolved_element = resolve_type_impl(array->element);
            return std::make_shared<ArrayType>(resolved_element ? resolved_element : array->element,
                                               array->size);
        }

        case Type::Kind::Struct: {
            auto* struct_type = type->as<StructType>();
            if (struct_type == nullptr) {
                return type;
            }

            std::vector<StructFieldType> resolved_fields;
            resolved_fields.reserve(struct_type->fields.size());
            for (const auto& field : struct_type->fields) {
                auto resolved_field_type = resolve_type_impl(field.type);
                resolved_fields.emplace_back(field.name, resolved_field_type ? resolved_field_type
                                                                             : field.type);
            }

            std::vector<GenericParameterType> resolved_params;
            resolved_params.reserve(struct_type->generic_parameters.size());
            for (const auto& generic_parameter : struct_type->generic_parameters) {
                std::shared_ptr<Type> resolved_constraint;
                if (generic_parameter.constraint) {
                    resolved_constraint = resolve_type_impl(generic_parameter.constraint);
                }
                resolved_params.emplace_back(generic_parameter.name, resolved_constraint);
            }

            return std::make_shared<StructType>(struct_type->name, std::move(resolved_params),
                                                std::move(resolved_fields));
        }

        case Type::Kind::Function: {
            auto* function_type = type->as<FunctionType>();
            if (function_type == nullptr) {
                return type;
            }

            auto resolved_return = resolve_type_impl(function_type->return_type);

            std::vector<ParameterType> resolved_parameters;
            resolved_parameters.reserve(function_type->parameters.size());
            for (const auto& parameter : function_type->parameters) {
                auto resolved_param_type = resolve_type_impl(parameter.type);
                resolved_parameters.emplace_back(
                    parameter.name, resolved_param_type ? resolved_param_type : parameter.type);
            }

            std::vector<GenericParameterType> resolved_generic_params;
            resolved_generic_params.reserve(function_type->generic_parameters.size());
            for (const auto& generic_parameter : function_type->generic_parameters) {
                std::shared_ptr<Type> resolved_constraint;
                if (generic_parameter.constraint) {
                    resolved_constraint = resolve_type_impl(generic_parameter.constraint);
                }
                resolved_generic_params.emplace_back(generic_parameter.name, resolved_constraint);
            }

            return std::make_shared<FunctionType>(
                resolved_return ? resolved_return : function_type->return_type,
                std::move(resolved_parameters), std::move(resolved_generic_params),
                function_type->variadic);
        }

        default:
            return type;
        }
    }

  public:
    explicit TypeContext(Env& env) : env(env) {}

    std::unordered_map<std::string, std::shared_ptr<Type>> save_substitutions() const {
        return generic_substitutions;
    }

    void restore_substitutions(std::unordered_map<std::string, std::shared_ptr<Type>> previous) {
        generic_substitutions = std::move(previous);
    }

    void add_substitution(const std::string& name, std::shared_ptr<Type> type) {
        generic_substitutions[name] = std::move(type);
    }

    bool has_substitutions() const { return !generic_substitutions.empty(); }

    const std::unordered_map<std::string, std::shared_ptr<Type>>& get_substitutions() const {
        return generic_substitutions;
    }

    std::shared_ptr<Type> resolve_type_name(const std::string& name) const {
        auto substitution = generic_substitutions.find(name);
        if (substitution != generic_substitutions.end()) {
            return substitution->second;
        }

        return env.lookup_type(name);
    }

    std::shared_ptr<Type> resolve_type(std::shared_ptr<Type> type) const {
        return resolve_type_impl(type);
    }

    static std::shared_ptr<Type> substitute_generic_type(
        std::shared_ptr<Type> type,
        const std::unordered_map<std::string, std::shared_ptr<Type>>& substitution_map) {
        if (!type) {
            return nullptr;
        }

        switch (type->kind) {

        case Type::Kind::Named: {
            auto* named = type->as<NamedType>();
            if (named) {
                auto iterator = substitution_map.find(named->name);
                if (iterator != substitution_map.end()) {
                    return iterator->second;
                }
            }
            break;
        }

        case Type::Kind::Pointer: {
            auto* pointer = type->as<PointerType>();
            if (pointer) {
                auto substituted = substitute_generic_type(pointer->element, substitution_map);
                if (substituted != pointer->element) {
                    return std::make_shared<PointerType>(std::move(substituted),
                                                         pointer->is_mutable);
                }
            }
            break;
        }

        case Type::Kind::Array: {
            auto* array = type->as<ArrayType>();
            if (array) {
                auto substituted = substitute_generic_type(array->element, substitution_map);
                if (substituted != array->element) {
                    return std::make_shared<ArrayType>(std::move(substituted), array->size);
                }
            }
            break;
        }

        case Type::Kind::Struct: {
            auto* struct_type = type->as<StructType>();
            if (struct_type) {
                std::vector<StructFieldType> substituted_fields;
                substituted_fields.reserve(struct_type->fields.size());
                bool changed = false;

                for (const auto& field : struct_type->fields) {
                    auto substituted = substitute_generic_type(field.type, substitution_map);
                    if (substituted != field.type) {
                        changed = true;
                    }
                    substituted_fields.emplace_back(field.name, std::move(substituted));
                }

                if (changed) {
                    return std::make_shared<StructType>(struct_type->name,
                                                        struct_type->generic_parameters,
                                                        std::move(substituted_fields));
                }
            }
            break;
        }

        case Type::Kind::Function: {
            auto* function_type = type->as<FunctionType>();
            if (function_type) {
                auto substituted_return =
                    substitute_generic_type(function_type->return_type, substitution_map);
                std::vector<ParameterType> substituted_parameters;
                substituted_parameters.reserve(function_type->parameters.size());
                bool changed = substituted_return != function_type->return_type;

                for (const auto& param : function_type->parameters) {
                    auto substituted = substitute_generic_type(param.type, substitution_map);
                    if (substituted != param.type) {
                        changed = true;
                    }
                    substituted_parameters.emplace_back(param.name, std::move(substituted));
                }

                if (changed) {
                    return std::make_shared<FunctionType>(
                        std::move(substituted_return), std::move(substituted_parameters),
                        function_type->generic_parameters, function_type->variadic);
                }
            }
            break;
        }

        default:
            break;
        }

        return type;
    }

    bool types_compatible(const std::shared_ptr<Type>& left,
                          const std::shared_ptr<Type>& right) const {
        if (!left || !right) {
            return true;
        }

        auto resolved_left = resolve_type(left);
        auto resolved_right = resolve_type(right);

        if (!resolved_left || !resolved_right) {
            return true;
        }

        if (resolved_left->kind == Type::Kind::Any || resolved_right->kind == Type::Kind::Any) {
            return true;
        }

        if (resolved_left->kind == Type::Kind::Unknown ||
            resolved_right->kind == Type::Kind::Unknown) {
            return true;
        }

        if (resolved_left->kind != resolved_right->kind) {
            return false;
        }

        switch (resolved_left->kind) {

        case Type::Kind::Integer: {
            auto* left_int = resolved_left->as<IntegerType>();
            auto* right_int = resolved_right->as<IntegerType>();
            if (left_int && right_int) {
                return left_int->is_unsigned == right_int->is_unsigned &&
                       left_int->size == right_int->size;
            }
            return false;
        }

        case Type::Kind::Float: {
            auto* left_float = resolved_left->as<FloatType>();
            auto* right_float = resolved_right->as<FloatType>();
            if (left_float && right_float) {
                return left_float->size == right_float->size;
            }
            return false;
        }

        case Type::Kind::Pointer: {
            auto* left_ptr = resolved_left->as<PointerType>();
            auto* right_ptr = resolved_right->as<PointerType>();
            if (left_ptr && right_ptr) {
                if (left_ptr->element->kind == Type::Kind::Void ||
                    right_ptr->element->kind == Type::Kind::Void) {
                    return true;
                }
                return types_compatible(left_ptr->element, right_ptr->element);
            }
            return false;
        }

        case Type::Kind::Array: {
            auto* left_arr = resolved_left->as<ArrayType>();
            auto* right_arr = resolved_right->as<ArrayType>();
            if (left_arr && right_arr) {
                if (left_arr->size != right_arr->size) {
                    return false;
                }
                return types_compatible(left_arr->element, right_arr->element);
            }
            return false;
        }

        case Type::Kind::Struct: {
            auto* left_struct = resolved_left->as<StructType>();
            auto* right_struct = resolved_right->as<StructType>();
            if (left_struct && right_struct) {
                if (left_struct->name != right_struct->name) {
                    return false;
                }

                if (left_struct->fields.size() != right_struct->fields.size()) {
                    return false;
                }

                for (std::size_t i = 0; i < left_struct->fields.size(); ++i) {
                    if (left_struct->fields[i].name != right_struct->fields[i].name) {
                        return false;
                    }
                    if (!types_compatible(left_struct->fields[i].type,
                                          right_struct->fields[i].type)) {
                        return false;
                    }
                }

                return true;
            }
            return false;
        }

        case Type::Kind::Function: {
            auto* left_func = resolved_left->as<FunctionType>();
            auto* right_func = resolved_right->as<FunctionType>();
            if (left_func && right_func) {
                if (!types_compatible(left_func->return_type, right_func->return_type)) {
                    return false;
                }
                if (left_func->parameters.size() != right_func->parameters.size()) {
                    return false;
                }
                for (std::size_t i = 0; i < left_func->parameters.size(); ++i) {
                    if (!types_compatible(left_func->parameters[i].type,
                                          right_func->parameters[i].type)) {
                        return false;
                    }
                }
                return true;
            }
            return false;
        }

        default:
            return true;
        }
    }
};
