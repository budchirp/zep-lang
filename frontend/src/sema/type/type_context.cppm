module;

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.frontend.sema.type.type_context;

import zep.frontend.sema.type;
import zep.frontend.sema.type.type_id;
import zep.frontend.sema.env;
import zep.frontend.arena.type;

export class TypeContext {
  private:
    TypeArena& type_arena;
    Env& env;

    std::unordered_map<std::string, TypeId> substitutions;

    TypeId resolve_type_name(const std::string& name) const {
        auto iterator = substitutions.find(name);
        if (iterator != substitutions.end()) {
            return iterator->second;
        }

        return env.current_scope->lookup_type(name);
    }

    TypeId substitute_type(TypeId id,
                           const std::unordered_map<std::string, TypeId>& substitution_map) {
        const Type* type = type_arena.get(id);
        if (type == nullptr) {
            return id;
        }

        switch (type->kind) {
        case Type::Kind::Type::Named: {
            const NamedType* named = type->as<NamedType>();
            auto iterator = substitution_map.find(named->name);
            if (iterator != substitution_map.end()) {
                return iterator->second;
            }
            return id;
        }

        case Type::Kind::Type::Pointer: {
            const PointerType* pointer = type->as<PointerType>();
            TypeId element = substitute_type(pointer->element, substitution_map);
            if (element == pointer->element) {
                return id;
            }
            return type_arena.create<PointerType>(element, pointer->is_mutable);
        }

        case Type::Kind::Type::Array: {
            const ArrayType* array = type->as<ArrayType>();
            TypeId element = substitute_type(array->element, substitution_map);
            if (element == array->element) {
                return id;
            }
            return type_arena.create<ArrayType>(element, array->size);
        }

        case Type::Kind::Type::Struct: {
            const StructType* struct_type = type->as<StructType>();

            std::vector<StructFieldType> new_fields;
            new_fields.reserve(struct_type->fields.size());
            bool changed = false;
            for (const StructFieldType& field : struct_type->fields) {
                TypeId substituted = substitute_type(field.type, substitution_map);
                if (substituted != field.type) {
                    changed = true;
                }
                new_fields.emplace_back(field.name, substituted);
            }

            if (!changed) {
                return id;
            }

            return type_arena.create<StructType>(struct_type->name,
                                                 struct_type->generic_parameters,
                                                 std::move(new_fields));
        }

        case Type::Kind::Type::Function: {
            const FunctionType* function_type = type->as<FunctionType>();
            TypeId return_type = substitute_type(function_type->return_type, substitution_map);

            std::vector<ParameterType> new_parameters;
            new_parameters.reserve(function_type->parameters.size());
            bool changed = return_type != function_type->return_type;
            for (const ParameterType& parameter : function_type->parameters) {
                TypeId substituted = substitute_type(parameter.type, substitution_map);
                if (substituted != parameter.type) {
                    changed = true;
                }
                new_parameters.emplace_back(parameter.name, substituted);
            }

            if (!changed) {
                return id;
            }

            return type_arena.create<FunctionType>(function_type->name, return_type,
                                                   std::move(new_parameters),
                                                   function_type->generic_parameters,
                                                   function_type->variadic);
        }

        default:
            return id;
        }
    }

  public:
    TypeContext(TypeArena& type_arena, Env& env) : type_arena(type_arena), env(env) {}

    class SubstitutionScope {
      private:
        TypeContext& context;
        std::unordered_map<std::string, TypeId> saved;

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

    void add_substitution(const std::string& name, TypeId type) { substitutions[name] = type; }

    TypeId resolve_type(TypeId id) {
        const Type* type = type_arena.get(id);
        if (type == nullptr) {
            return id;
        }

        switch (type->kind) {
        case Type::Kind::Type::Named: {
            const NamedType* named = type->as<NamedType>();
            TypeId base = resolve_type_name(named->name);
            if (!base.is_valid()) {
                return id;
            }

            if (!named->generic_arguments.empty()) {
                const Type* base_type = type_arena.get(base);
                const StructType* struct_type = base_type->as<StructType>();
                if (struct_type != nullptr &&
                    named->generic_arguments.size() == struct_type->generic_parameters.size()) {
                    std::unordered_map<std::string, TypeId> substitution_map;
                    for (std::size_t i = 0; i < named->generic_arguments.size(); ++i) {
                        substitution_map[struct_type->generic_parameters[i].name] =
                            resolve_type(named->generic_arguments[i].type);
                    }
                    return resolve_type(substitute_type(base, substitution_map));
                }
            }

            return resolve_type(base);
        }

        case Type::Kind::Type::Pointer: {
            const PointerType* pointer = type->as<PointerType>();
            TypeId element = resolve_type(pointer->element);
            if (element == pointer->element) {
                return id;
            }
            return type_arena.create<PointerType>(element, pointer->is_mutable);
        }

        case Type::Kind::Type::Array: {
            const ArrayType* array = type->as<ArrayType>();
            TypeId element = resolve_type(array->element);
            if (element == array->element) {
                return id;
            }
            return type_arena.create<ArrayType>(element, array->size);
        }

        case Type::Kind::Type::Struct: {
            const StructType* struct_type = type->as<StructType>();

            std::vector<StructFieldType> resolved_fields;
            resolved_fields.reserve(struct_type->fields.size());
            for (const StructFieldType& field : struct_type->fields) {
                TypeId resolved_field_type = resolve_type(field.type);
                resolved_fields.emplace_back(field.name, resolved_field_type);
            }

            std::vector<GenericParameterType> resolved_params;
            resolved_params.reserve(struct_type->generic_parameters.size());
            for (const GenericParameterType& generic_parameter : struct_type->generic_parameters) {
                TypeId resolved_constraint = TypeId{};
                if (generic_parameter.constraint.is_valid()) {
                    resolved_constraint = resolve_type(generic_parameter.constraint);
                }
                resolved_params.emplace_back(generic_parameter.name, resolved_constraint);
            }

            bool changed = false;
            for (std::size_t i = 0; i < struct_type->fields.size(); ++i) {
                if (resolved_fields[i].type != struct_type->fields[i].type) {
                    changed = true;
                    break;
                }
            }
            if (!changed) {
                for (std::size_t i = 0; i < struct_type->generic_parameters.size(); ++i) {
                    TypeId old_c = struct_type->generic_parameters[i].constraint;
                    TypeId new_c = resolved_params[i].constraint;
                    if (old_c != new_c) {
                        changed = true;
                        break;
                    }
                }
            }

            if (!changed) {
                return id;
            }

            return type_arena.create<StructType>(struct_type->name, std::move(resolved_params),
                                                 std::move(resolved_fields));
        }

        case Type::Kind::Type::Function: {
            const FunctionType* function_type = type->as<FunctionType>();

            TypeId resolved_return = resolve_type(function_type->return_type);

            std::vector<ParameterType> resolved_parameters;
            resolved_parameters.reserve(function_type->parameters.size());
            for (const ParameterType& parameter : function_type->parameters) {
                TypeId resolved_param_type = resolve_type(parameter.type);
                resolved_parameters.emplace_back(parameter.name, resolved_param_type);
            }

            std::vector<GenericParameterType> resolved_generic_params;
            resolved_generic_params.reserve(function_type->generic_parameters.size());
            for (const GenericParameterType& generic_parameter :
                 function_type->generic_parameters) {
                TypeId resolved_constraint = TypeId{};
                if (generic_parameter.constraint.is_valid()) {
                    resolved_constraint = resolve_type(generic_parameter.constraint);
                }
                resolved_generic_params.emplace_back(generic_parameter.name, resolved_constraint);
            }

            bool changed = resolved_return != function_type->return_type;
            if (!changed) {
                for (std::size_t i = 0; i < function_type->parameters.size(); ++i) {
                    if (resolved_parameters[i].type != function_type->parameters[i].type) {
                        changed = true;
                        break;
                    }
                }
            }
            if (!changed) {
                for (std::size_t i = 0; i < function_type->generic_parameters.size(); ++i) {
                    TypeId old_c = function_type->generic_parameters[i].constraint;
                    TypeId new_c = resolved_generic_params[i].constraint;
                    if (old_c != new_c) {
                        changed = true;
                        break;
                    }
                }
            }

            if (!changed) {
                return id;
            }

            return type_arena.create<FunctionType>(
                function_type->name, resolved_return, std::move(resolved_parameters),
                std::move(resolved_generic_params), function_type->variadic);
        }

        default:
            return id;
        }
    }
};
