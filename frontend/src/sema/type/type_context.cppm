module;

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.frontend.sema.type.type_context;

import zep.frontend.sema.type;
import zep.frontend.sema.env;
import zep.frontend.sema.scope;
import zep.frontend.arena;

export class TypeContext {
  private:
    TypeArena& type_arena;
    Env& env;
    std::unordered_map<std::string, const Type*> substitutions;

    const Type* substitute_type(const Type* type,
                    const std::unordered_map<std::string, const Type*>& substitution_map) {
        if (type == nullptr) {
            return nullptr;
        }

        switch (type->kind) {
        case Type::Kind::Type::Named: {
            const auto* named = type->as<NamedType>();
            auto iterator = substitution_map.find(named->name);

            if (iterator != substitution_map.end()) {
                return iterator->second;
            }

            return type;
        }

        case Type::Kind::Type::Pointer: {
            const auto* pointer = type->as<PointerType>();
            const auto* element = substitute_type(pointer->element, substitution_map);

            if (element == pointer->element) {
                return type;
            }

            return type_arena.create<PointerType>(element, pointer->is_mutable);
        }

        case Type::Kind::Type::Array: {
            const auto* array = type->as<ArrayType>();
            const auto* element = substitute_type(array->element, substitution_map);

            if (element == array->element) {
                return type;
            }

            return type_arena.create<ArrayType>(element, array->size);
        }

        case Type::Kind::Type::Struct: {
            const auto* struct_type = type->as<StructType>();

            std::vector<StructFieldType> new_fields;
            new_fields.reserve(struct_type->fields.size());
            bool changed = false;
            for (const StructFieldType& field : struct_type->fields) {
                const auto* substituted = substitute_type(field.type, substitution_map);

                if (substituted != field.type) {
                    changed = true;
                }

                new_fields.emplace_back(field.name, substituted);
            }

            if (!changed) {
                return type;
            }

            return type_arena.create<StructType>(
                struct_type->name,
                struct_type->generic_parameters,
                std::move(new_fields));
        }

        case Type::Kind::Type::Function: {
            const auto* function_type = type->as<FunctionType>();
            const auto* return_type = substitute_type(function_type->return_type, substitution_map);

            std::vector<ParameterType> new_parameters;
            new_parameters.reserve(function_type->parameters.size());
            bool changed = return_type != function_type->return_type;
            for (const ParameterType& parameter : function_type->parameters) {
                const auto* substituted = substitute_type(parameter.type, substitution_map);

                if (substituted != parameter.type) {
                    changed = true;
                }

                new_parameters.emplace_back(parameter.name, substituted);
            }

            if (!changed) {
                return type;
            }

            return type_arena.create<FunctionType>(
                function_type->name,
                return_type,
                std::move(new_parameters),
                function_type->generic_parameters,
                function_type->variadic);
        }

        default:
            return type;
        }
    }

  public:
    TypeContext(TypeArena& type_arena, Env& env) : type_arena(type_arena), env(env) {}

    class SubstitutionScope {
      private:
        TypeContext& context;
        std::unordered_map<std::string, const Type*> saved;

      public:
        explicit SubstitutionScope(TypeContext& context)
            : context(context), saved(context.substitutions) {}

        SubstitutionScope(const SubstitutionScope&) = delete;
        SubstitutionScope& operator=(const SubstitutionScope&) = delete;
        SubstitutionScope(SubstitutionScope&&) = delete;
        SubstitutionScope& operator=(SubstitutionScope&&) = delete;

        ~SubstitutionScope() { context.substitutions = std::move(saved); }
    };

    [[nodiscard]] SubstitutionScope scoped_substitutions() { return SubstitutionScope(*this); }

    void add_substitution(const std::string& name, const Type* type) { substitutions[name] = type; }

    const Type* resolve_type(const Type* type) {
        if (type == nullptr) {
            return nullptr;
        }

        switch (type->kind) {
        case Type::Kind::Type::Named: {
            const auto* named = type->as<NamedType>();

            auto iterator = substitutions.find(named->name);
            if (iterator != substitutions.end()) {
                return iterator->second;
            }

            auto* symbol = env.current_scope->lookup_type(named->name);
            if (symbol == nullptr) {
                return nullptr;
            }

            auto* resolved_symbol_type = symbol->type;

            if (!named->generic_arguments.empty()) {
                const auto* struct_type = resolved_symbol_type->as<StructType>();
                if (struct_type == nullptr) {
                                        return nullptr;
                }

                if (named->generic_arguments.size() == struct_type->generic_parameters.size()) {
                    std::unordered_map<std::string, const Type*> substitution_map;

                    for (std::size_t i = 0; i < named->generic_arguments.size(); ++i) {
                        substitution_map[struct_type->generic_parameters[i].name] =
                            resolve_type(named->generic_arguments[i].type);
                    }

                    return resolve_type(substitute_type(resolved_symbol_type, substitution_map));
                }
            }

            return resolved_symbol_type;
        }

        case Type::Kind::Type::Pointer: {
            const auto* pointer = type->as<PointerType>();
            const auto* element = resolve_type(pointer->element);

            if (element == pointer->element) {
                return type;
            }

            return type_arena.create<PointerType>(element, pointer->is_mutable);
        }

        case Type::Kind::Type::Array: {
            const auto* array = type->as<ArrayType>();
            const auto* element = resolve_type(array->element);

            if (element == array->element) {
                return type;
            }

            return type_arena.create<ArrayType>(element, array->size);
        }

        case Type::Kind::Type::Struct: {
            const auto* struct_type = type->as<StructType>();

            std::vector<StructFieldType> resolved_fields;
            resolved_fields.reserve(struct_type->fields.size());
            bool changed = false;

            for (const StructFieldType& field : struct_type->fields) {
                const auto* resolved_field_type = resolve_type(field.type);

                if (resolved_field_type != field.type) {
                    changed = true;
                }

                resolved_fields.emplace_back(field.name, resolved_field_type);
            }

            std::vector<GenericParameterType> resolved_params;
            resolved_params.reserve(struct_type->generic_parameters.size());

            for (const GenericParameterType& generic_parameter : struct_type->generic_parameters) {
                const Type* resolved_constraint = nullptr;

                if (generic_parameter.constraint != nullptr) {
                    resolved_constraint = resolve_type(generic_parameter.constraint);
                }

                if (resolved_constraint != generic_parameter.constraint) {
                    changed = true;
                }

                resolved_params.emplace_back(generic_parameter.name, resolved_constraint);
            }

            if (!changed) {
                return type;
            }

            return type_arena.create<StructType>(
                struct_type->name,
                std::move(resolved_params),
                std::move(resolved_fields));
        }

        case Type::Kind::Type::Function: {
            const auto* function_type = type->as<FunctionType>();

            const auto* resolved_return = resolve_type(function_type->return_type);
            bool changed = resolved_return != function_type->return_type;

            std::vector<ParameterType> resolved_parameters;
            resolved_parameters.reserve(function_type->parameters.size());

            for (const ParameterType& parameter : function_type->parameters) {
                const auto* resolved_param_type = resolve_type(parameter.type);

                if (resolved_param_type != parameter.type) {
                    changed = true;
                }

                resolved_parameters.emplace_back(parameter.name, resolved_param_type);
            }

            std::vector<GenericParameterType> resolved_generic_params;
            resolved_generic_params.reserve(function_type->generic_parameters.size());

            for (const GenericParameterType& generic_parameter :
                 function_type->generic_parameters) {
                const Type* resolved_constraint = nullptr;

                if (generic_parameter.constraint != nullptr) {
                    resolved_constraint = resolve_type(generic_parameter.constraint);
                }

                if (resolved_constraint != generic_parameter.constraint) {
                    changed = true;
                }

                resolved_generic_params.emplace_back(generic_parameter.name, resolved_constraint);
            }

            if (!changed) {
                return type;
            }

            return type_arena.create<FunctionType>(
                function_type->name,
                resolved_return,
                std::move(resolved_parameters),
                std::move(resolved_generic_params),
                function_type->variadic);
        }

        default:
            return type;
        }
    }
};
