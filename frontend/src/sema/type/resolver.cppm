module;

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.frontend.sema.type.resolver;

import zep.frontend.sema.type;
import zep.frontend.sema.env;
import zep.frontend.sema.scope;
import zep.frontend.arena;

export class TypeResolver {
  private:
    TypeArena& type_arena;
    Env& env;

    std::unordered_map<std::string, const Type*> substitutions;

    std::vector<GenericParameterType>
    substitute_generic_parameters(const std::vector<GenericParameterType>& parameters,
                                  const std::unordered_map<std::string, const Type*>& map,
                                  bool& changed) {
        std::vector<GenericParameterType> result;
        result.reserve(parameters.size());

        for (const auto& parameter : parameters) {
            if (map.contains(parameter.name)) {
                changed = true;
                continue;
            }

            const auto* substituted = substitute_type(parameter.constraint, map);
            if (substituted != parameter.constraint) {
                changed = true;
            }

            result.emplace_back(parameter.name, substituted);
        }

        return result;
    }

    std::vector<StructFieldType>
    substitute_fields(const std::vector<StructFieldType>& fields,
                      const std::unordered_map<std::string, const Type*>& map, bool& changed) {
        std::vector<StructFieldType> result;
        result.reserve(fields.size());

        for (const auto& field : fields) {
            const auto* substituted = substitute_type(field.type, map);
            if (substituted != field.type) {
                changed = true;
            }

            result.emplace_back(field.name, substituted);
        }

        return result;
    }

    std::vector<ParameterType>
    substitute_parameters(const std::vector<ParameterType>& parameters,
                          const std::unordered_map<std::string, const Type*>& map, bool& changed) {
        std::vector<ParameterType> result;
        result.reserve(parameters.size());

        for (const auto& parameter : parameters) {
            const auto* substituted = substitute_type(parameter.type, map);
            if (substituted != parameter.type) {
                changed = true;
            }

            result.emplace_back(parameter.name, substituted);
        }

        return result;
    }

    const Type*
    substitute_type(const Type* type,
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

            bool changed = false;
            std::vector<GenericArgumentType> new_args;
            new_args.reserve(named->generic_arguments.size());

            for (const auto& arg : named->generic_arguments) {
                const auto* substituted = substitute_type(arg.type, substitution_map);
                if (substituted != arg.type) {
                    changed = true;
                }
                new_args.emplace_back(arg.name, substituted);
            }

            if (!changed) {
                return type;
            }

            return type_arena.create<NamedType>(named->name, std::move(new_args));
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

            bool changed = false;
            auto new_params = substitute_generic_parameters(struct_type->generic_parameters,
                                                            substitution_map, changed);
            auto new_fields = substitute_fields(struct_type->fields, substitution_map, changed);

            if (!changed) {
                return type;
            }

            return type_arena.create<StructType>(struct_type->name, std::move(new_params),
                                                 std::move(new_fields));
        }

        case Type::Kind::Type::Function: {
            const auto* function_type = type->as<FunctionType>();
            const auto* return_type = substitute_type(function_type->return_type, substitution_map);

            bool changed = return_type != function_type->return_type;
            auto new_parameters =
                substitute_parameters(function_type->parameters, substitution_map, changed);
            auto new_params = substitute_generic_parameters(function_type->generic_parameters,
                                                            substitution_map, changed);

            if (!changed) {
                return type;
            }

            return type_arena.create<FunctionType>(function_type->name, return_type,
                                                   std::move(new_parameters), std::move(new_params),
                                                   function_type->variadic);
        }

        default:
            return type;
        }
    }

    std::vector<GenericParameterType>
    resolve_generic_parameters(const std::vector<GenericParameterType>& parameters, bool& changed) {
        std::vector<GenericParameterType> result;
        result.reserve(parameters.size());

        for (const auto& parameter : parameters) {
            if (substitutions.contains(parameter.name)) {
                changed = true;
                continue;
            }

            const auto* resolved = resolve(parameter.constraint);
            if (resolved != parameter.constraint) {
                changed = true;
            }

            result.emplace_back(parameter.name, resolved);
        }

        return result;
    }

  public:
    TypeResolver(TypeArena& type_arena, Env& env) : type_arena(type_arena), env(env) {}

    class SubstitutionScope {
      private:
        TypeResolver& context;
        std::unordered_map<std::string, const Type*> saved;

      public:
        explicit SubstitutionScope(TypeResolver& context)
            : context(context), saved(context.substitutions) {}

        SubstitutionScope(const SubstitutionScope&) = delete;
        SubstitutionScope& operator=(const SubstitutionScope&) = delete;
        SubstitutionScope(SubstitutionScope&&) = delete;
        SubstitutionScope& operator=(SubstitutionScope&&) = delete;

        ~SubstitutionScope() { context.substitutions = std::move(saved); }
    };

    [[nodiscard]] SubstitutionScope scoped_substitutions() { return SubstitutionScope(*this); }

    void add_substitution(const std::string& name, const Type* type) { substitutions[name] = type; }

    const Type* resolve(const Type* type) {
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
                return type;
            }

            auto* resolved_symbol_type = symbol->type;

            if (!named->generic_arguments.empty()) {
                const auto* struct_type = resolved_symbol_type->as<StructType>();
                if (struct_type != nullptr &&
                    named->generic_arguments.size() == struct_type->generic_parameters.size()) {
                    std::unordered_map<std::string, const Type*> substitution_map;

                    for (std::size_t i = 0; i < named->generic_arguments.size(); ++i) {
                        substitution_map[struct_type->generic_parameters[i].name] =
                            resolve(named->generic_arguments[i].type);
                    }

                    return resolve(substitute_type(resolved_symbol_type, substitution_map));
                }
            }

            return resolved_symbol_type;
        }

        case Type::Kind::Type::Pointer: {
            const auto* pointer = type->as<PointerType>();
            const auto* element = resolve(pointer->element);

            if (element == pointer->element) {
                return type;
            }

            return type_arena.create<PointerType>(element, pointer->is_mutable);
        }

        case Type::Kind::Type::Array: {
            const auto* array = type->as<ArrayType>();
            const auto* element = resolve(array->element);

            if (element == array->element) {
                return type;
            }

            return type_arena.create<ArrayType>(element, array->size);
        }

        case Type::Kind::Type::Struct: {
            const auto* struct_type = type->as<StructType>();

            bool changed = false;

            std::vector<StructFieldType> resolved_fields;
            resolved_fields.reserve(struct_type->fields.size());

            for (const StructFieldType& field : struct_type->fields) {
                const auto* resolved_field_type = resolve(field.type);
                if (resolved_field_type != field.type) {
                    changed = true;
                }
                resolved_fields.emplace_back(field.name, resolved_field_type);
            }

            auto resolved_params =
                resolve_generic_parameters(struct_type->generic_parameters, changed);

            if (!changed) {
                return type;
            }

            return type_arena.create<StructType>(struct_type->name, std::move(resolved_params),
                                                 std::move(resolved_fields));
        }

        case Type::Kind::Type::Function: {
            const auto* function_type = type->as<FunctionType>();

            const auto* resolved_return = resolve(function_type->return_type);
            bool changed = resolved_return != function_type->return_type;

            std::vector<ParameterType> resolved_parameters;
            resolved_parameters.reserve(function_type->parameters.size());

            for (const ParameterType& parameter : function_type->parameters) {
                const auto* resolved_param_type = resolve(parameter.type);
                if (resolved_param_type != parameter.type) {
                    changed = true;
                }
                resolved_parameters.emplace_back(parameter.name, resolved_param_type);
            }

            auto resolved_generic_params =
                resolve_generic_parameters(function_type->generic_parameters, changed);

            if (!changed) {
                return type;
            }

            return type_arena.create<FunctionType>(
                function_type->name, resolved_return, std::move(resolved_parameters),
                std::move(resolved_generic_params), function_type->variadic);
        }

        default:
            return type;
        }
    }
};
