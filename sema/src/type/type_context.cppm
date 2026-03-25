module;

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.sema.type.type_context;

import zep.sema.type;
import zep.sema.env;

export class TypeContext {
  private:
    Env& env;

    std::unordered_map<std::string, std::shared_ptr<Type>> substitutions;

    std::shared_ptr<Type> resolve_type_name(const std::string& name) const {
        auto iterator = substitutions.find(name);
        if (iterator != substitutions.end()) {
            return iterator->second;
        }

        return env.current_scope->lookup_type(name);
    }

    static std::shared_ptr<Type> substitute_type(
        std::shared_ptr<Type> type,
        const std::unordered_map<std::string, std::shared_ptr<Type>>& substitution_map) {
        if (!type) {
            return nullptr;
        }

        switch (type->kind) {

        case Type::Kind::Type::Named: {
            auto* named = type->as<NamedType>();
            if (named != nullptr) {
                auto iterator = substitution_map.find(named->name);
                if (iterator != substitution_map.end()) {
                    return iterator->second;
                }
            }
            break;
        }

        case Type::Kind::Type::Pointer: {
            auto* pointer = type->as<PointerType>();
            if (pointer != nullptr) {
                auto substituted = substitute_type(pointer->element, substitution_map);
                if (substituted != pointer->element) {
                    return std::make_shared<PointerType>(std::move(substituted),
                                                         pointer->is_mutable);
                }
            }
            break;
        }

        case Type::Kind::Type::Array: {
            auto* array = type->as<ArrayType>();
            if (array != nullptr) {
                auto substituted = substitute_type(array->element, substitution_map);
                if (substituted != array->element) {
                    return std::make_shared<ArrayType>(std::move(substituted), array->size);
                }
            }
            break;
        }

        case Type::Kind::Type::Struct: {
            auto* struct_type = type->as<StructType>();
            if (struct_type != nullptr) {
                std::vector<std::shared_ptr<StructFieldType>> substituted_fields;
                substituted_fields.reserve(struct_type->fields.size());
                bool changed = false;

                for (const auto& field : struct_type->fields) {
                    auto substituted = substitute_type(field->type, substitution_map);
                    if (substituted != field->type) {
                        changed = true;
                    }
                    substituted_fields.emplace_back(
                        std::make_shared<StructFieldType>(field->name, std::move(substituted)));
                }

                if (changed) {
                    return std::make_shared<StructType>(struct_type->name,
                                                        struct_type->generic_parameters,
                                                        std::move(substituted_fields));
                }
            }
            break;
        }

        case Type::Kind::Type::Function: {
            auto* function_type = type->as<FunctionType>();
            if (function_type != nullptr) {
                auto substituted_return =
                    substitute_type(function_type->return_type, substitution_map);
                std::vector<std::shared_ptr<ParameterType>> substituted_parameters;
                substituted_parameters.reserve(function_type->parameters.size());
                bool changed = substituted_return != function_type->return_type;

                for (const auto& parameter : function_type->parameters) {
                    auto substituted = substitute_type(parameter->type, substitution_map);
                    if (substituted != parameter->type) {
                        changed = true;
                    }
                    substituted_parameters.emplace_back(
                        std::make_shared<ParameterType>(parameter->name, std::move(substituted)));
                }

                if (changed) {
                    return std::make_shared<FunctionType>(
                        function_type->name, std::move(substituted_return),
                        std::move(substituted_parameters), function_type->generic_parameters,
                        function_type->variadic);
                }
            }
            break;
        }

        default:
            break;
        }

        return type;
    }

  public:
    explicit TypeContext(Env& env) : env(env) {}

    class SubstitutionScope {
      private:
        TypeContext& context;
        std::unordered_map<std::string, std::shared_ptr<Type>> saved;

      public:
        explicit SubstitutionScope(TypeContext& context)
            : context(context), saved(context.substitutions) {}

        ~SubstitutionScope() { context.substitutions = std::move(saved); }

        SubstitutionScope(const SubstitutionScope&) = delete;
        SubstitutionScope& operator=(const SubstitutionScope&) = delete;
        SubstitutionScope(SubstitutionScope&&) = delete;
        SubstitutionScope& operator=(SubstitutionScope&&) = delete;
    };

    [[nodiscard]] SubstitutionScope scoped_substitutions() { return SubstitutionScope(*this); }

    void add_substitution(const std::string& name, std::shared_ptr<Type> type) {
        substitutions[name] = std::move(type);
    }

    std::shared_ptr<Type> resolve_type(std::shared_ptr<Type> type) const {
        if (!type) {
            return nullptr;
        }

        switch (type->kind) {

        case Type::Kind::Type::Named: {
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
                        substitution_map[struct_type->generic_parameters[i]->name] =
                            resolve_type(named->generic_arguments[i]->type);
                    }
                    return resolve_type(substitute_type(base, substitution_map));
                }
            }

            return resolve_type(base);
        }

        case Type::Kind::Type::Pointer: {
            auto* pointer = type->as<PointerType>();
            if (pointer == nullptr) {
                return type;
            }

            auto resolved_element = resolve_type(pointer->element);
            return std::make_shared<PointerType>(
                resolved_element ? resolved_element : pointer->element, pointer->is_mutable);
        }

        case Type::Kind::Type::Array: {
            auto* array = type->as<ArrayType>();
            if (array == nullptr) {
                return type;
            }

            auto resolved_element = resolve_type(array->element);
            return std::make_shared<ArrayType>(resolved_element ? resolved_element : array->element,
                                               array->size);
        }

        case Type::Kind::Type::Struct: {
            auto* struct_type = type->as<StructType>();
            if (struct_type == nullptr) {
                return type;
            }

            std::vector<std::shared_ptr<StructFieldType>> resolved_fields;
            resolved_fields.reserve(struct_type->fields.size());
            for (const auto& field : struct_type->fields) {
                auto resolved_field_type = resolve_type(field->type);
                resolved_fields.emplace_back(std::make_shared<StructFieldType>(
                    field->name, resolved_field_type ? resolved_field_type : field->type));
            }

            std::vector<std::shared_ptr<GenericParameterType>> resolved_params;
            resolved_params.reserve(struct_type->generic_parameters.size());
            for (const auto& generic_parameter : struct_type->generic_parameters) {
                std::shared_ptr<Type> resolved_constraint;
                if (generic_parameter->constraint) {
                    resolved_constraint = resolve_type(generic_parameter->constraint);
                }
                resolved_params.emplace_back(std::make_shared<GenericParameterType>(
                    generic_parameter->name, std::move(resolved_constraint)));
            }

            return std::make_shared<StructType>(struct_type->name, std::move(resolved_params),
                                                std::move(resolved_fields));
        }

        case Type::Kind::Type::Function: {
            auto* function_type = type->as<FunctionType>();
            if (function_type == nullptr) {
                return type;
            }

            auto resolved_return = resolve_type(function_type->return_type);

            std::vector<std::shared_ptr<ParameterType>> resolved_parameters;
            resolved_parameters.reserve(function_type->parameters.size());
            for (const auto& parameter : function_type->parameters) {
                auto resolved_param_type = resolve_type(parameter->type);
                resolved_parameters.emplace_back(std::make_shared<ParameterType>(
                    parameter->name, resolved_param_type ? resolved_param_type : parameter->type));
            }

            std::vector<std::shared_ptr<GenericParameterType>> resolved_generic_params;
            resolved_generic_params.reserve(function_type->generic_parameters.size());
            for (const auto& generic_parameter : function_type->generic_parameters) {
                std::shared_ptr<Type> resolved_constraint;
                if (generic_parameter->constraint) {
                    resolved_constraint = resolve_type(generic_parameter->constraint);
                }
                resolved_generic_params.emplace_back(std::make_shared<GenericParameterType>(
                    generic_parameter->name, std::move(resolved_constraint)));
            }

            return std::make_shared<FunctionType>(
                function_type->name, resolved_return ? resolved_return : function_type->return_type,
                std::move(resolved_parameters), std::move(resolved_generic_params),
                function_type->variadic);
        }

        default:
            return type;
        }
    }
};
