module;

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

export module zep.frontend.sema.type.resolver;

import zep.frontend.sema.type;
import zep.frontend.sema.env;
import zep.frontend.sema.scope;
import zep.frontend.sema.kind;
import zep.common.diagnostic.diagnostic;
import zep.common.diagnostic.collection;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.constant.environment;
import zep.frontend.sema.constant.evaluator;

export class TypeResolver {
  private:
    TypeArena& type_arena;
    Env& env;
    Diagnostics& diagnostics;

    CompileTimeEnvironment substitutions;

    struct FunctionCacheKeyHash {
        std::size_t operator()(const std::pair<const FunctionType*, const Scope*>& key) const {
            return std::hash<const FunctionType*>{}(key.first) ^
                   (std::hash<const Scope*>{}(key.second) << 1);
        }
    };

    std::unordered_map<std::pair<const FunctionType*, const Scope*>, const FunctionType*,
                       FunctionCacheKeyHash>
        function_type_cache;

    class NominalCacheKey {
      public:
        const Type* base;
        std::vector<GenericBinding> arguments;

        NominalCacheKey(const Type* base, std::vector<GenericBinding> arguments)
            : base(base), arguments(std::move(arguments)) {}

        bool operator==(const NominalCacheKey& other) const {
            return base == other.base && arguments == other.arguments;
        }
    };

    class NominalCacheKeyHash {
      public:
        std::size_t operator()(const NominalCacheKey& key) const {
            auto value = std::hash<const Type*>{}(key.base);

            for (const auto& argument : key.arguments) {
                value ^= GenericBindingHash{}(argument) + 0x9e3779b9 + (value << 6) + (value >> 2);
            }

            return value;
        }
    };

    std::unordered_set<const Type*> resolving_types;
    class SubstitutionKey {
      public:
        const Type* type;
        const CompileTimeEnvironment* environment;

        SubstitutionKey(const Type* type, const CompileTimeEnvironment* environment)
            : type(type), environment(environment) {}

        bool operator==(const SubstitutionKey&) const = default;
    };

    class SubstitutionHash {
      public:
        std::size_t operator()(const SubstitutionKey& key) const {
            auto result = std::hash<const Type*>()(key.type);
            result ^= std::hash<const CompileTimeEnvironment*>()(key.environment) + 0x9e3779b9 +
                      (result << 6) + (result >> 2);
            return result;
        }
    };

    std::unordered_set<SubstitutionKey, SubstitutionHash> active_substitutions;

    using NominalCache = std::unordered_map<NominalCacheKey, const Type*, NominalCacheKeyHash>;

    static NominalCache& nominal_type_cache() {
        static NominalCache cache;
        return cache;
    }

    static void clear_nominal_type_cache() { nominal_type_cache().clear(); }

    class ResolvingGuard {
      private:
        std::unordered_set<const Type*>& set;
        const Type* type;

      public:
        bool already_present;

        ResolvingGuard(std::unordered_set<const Type*>& set, const Type* type)
            : set(set), type(type) {
            already_present = !set.insert(type).second;
        }

        ~ResolvingGuard() {
            if (!already_present) {
                set.erase(type);
            }
        }
    };

    class SubstitutionGuard {
      private:
        std::unordered_set<SubstitutionKey, SubstitutionHash>& set;
        SubstitutionKey key;

      public:
        bool already_present;

        SubstitutionGuard(std::unordered_set<SubstitutionKey, SubstitutionHash>& set,
                          SubstitutionKey key)
            : set(set), key(std::move(key)) {
            already_present = !set.insert(this->key).second;
        }

        ~SubstitutionGuard() {
            if (!already_present) {
                set.erase(key);
            }
        }
    };

    const TypeSymbol* lookup_type_symbol(const std::string& name) const {
        if (auto* symbol = env.current_scope->lookup_type(name); symbol != nullptr) {
            return symbol;
        }

        return nullptr;
    }

    ConstBinding substitute_const_binding(const ConstBinding& binding,
                                          const CompileTimeEnvironment& environment) {
        if (binding.value.has_value() || binding.source == nullptr) {
            return binding;
        }

        if (!binding.source_is_expression) {
            const auto* generic = environment.lookup(binding.source);
            const auto* value = generic != nullptr ? std::get_if<ConstBinding>(generic) : nullptr;
            return value != nullptr && *value != binding
                       ? substitute_const_binding(*value, environment)
                       : binding;
        }

        Evaluator evaluator(diagnostics, &env, &environment, true);
        auto* expression = const_cast<Expression*>(static_cast<const Expression*>(binding.source));
        const auto result = evaluator.evaluate_uncached(*expression);
        return result.has_value() ? ConstBinding(*result) : binding;
    }

    std::vector<GenericParameterType>
    substitute_generic_parameters(const std::vector<GenericParameterType>& parameters,
                                  const CompileTimeEnvironment& substitution_map, bool& changed) {
        std::vector<GenericParameterType> result;
        result.reserve(parameters.size());

        for (const auto& parameter : parameters) {
            if (substitution_map.contains(parameter)) {
                changed = true;
                continue;
            }

            const auto* parameter_type = substitute(parameter.type, substitution_map);
            if (parameter_type != parameter.type) {
                changed = true;
            }

            result.emplace_back(parameter.kind, parameter.name, parameter_type,
                                parameter.declaration);
        }

        return result;
    }

    std::vector<GenericArgumentType>
    substitute_generic_arguments(const std::vector<GenericArgumentType>& arguments,
                                 const CompileTimeEnvironment& substitution_map, bool& changed) {
        std::vector<GenericArgumentType> result;
        result.reserve(arguments.size());

        for (const auto& argument : arguments) {
            if (argument.is_const()) {
                auto binding = substitute_const_binding(*argument.const_binding, substitution_map);
                changed = changed || binding != *argument.const_binding;
                result.emplace_back(argument.name, std::move(binding));
                continue;
            }

            const auto* type = substitute(argument.type, substitution_map);
            if (type != argument.type) {
                changed = true;
            }

            result.emplace_back(argument.name, type);
        }

        return result;
    }

    std::vector<FieldType> substitute_fields(const std::vector<FieldType>& fields,
                                             const CompileTimeEnvironment& substitution_map,
                                             bool& changed) {
        std::vector<FieldType> result;
        result.reserve(fields.size());

        for (const auto& field : fields) {
            const auto* type = substitute(field.type, substitution_map);
            if (type != field.type) {
                changed = true;
            }

            result.emplace_back(field.name, type, field.visibility);
        }

        return result;
    }

    std::vector<MethodType> substitute_methods(const std::vector<MethodType>& methods,
                                               const CompileTimeEnvironment& substitution_map,
                                               bool& changed) {
        std::vector<MethodType> result;
        result.reserve(methods.size());

        for (const auto& method : methods) {
            const auto* type = substitute(method.type, substitution_map);
            const auto* function_type = type != nullptr ? type->as<FunctionType>() : nullptr;
            if (function_type != method.type) {
                changed = true;
            }

            result.emplace_back(method.name, function_type, method.index);
        }

        return result;
    }

    std::vector<GenericParameterType>
    resolve_generic_parameters(const std::vector<GenericParameterType>& parameters, bool& changed) {
        std::vector<GenericParameterType> result;
        result.reserve(parameters.size());

        for (const auto& parameter : parameters) {
            if (parameter.is_const()) {
                if (substitutions.contains(parameter)) {
                    changed = true;
                    continue;
                }

                const auto* value_type = resolve_type(parameter.type);
                if (value_type != parameter.type) {
                    changed = true;
                }

                result.emplace_back(GenericParameterType::Kind::Type::Const, parameter.name,
                                    value_type, parameter.declaration);
                continue;
            }

            if (substitutions.contains(parameter)) {
                changed = true;
                continue;
            }

            const auto* constraint = resolve_type(parameter.type);
            if (constraint != parameter.type) {
                changed = true;
            }

            result.emplace_back(parameter.name, constraint, parameter.declaration);
        }

        return result;
    }

    std::optional<std::vector<ParameterType>>
    resolve_parameters(const std::vector<ParameterType>& parameters) {
        std::optional<std::vector<ParameterType>> result;

        for (std::size_t index = 0; index < parameters.size(); ++index) {
            const auto& parameter = parameters[index];
            const auto* type = resolve_type(parameter.type);
            if (type == parameter.type) {
                if (result.has_value()) {
                    result->push_back(parameter);
                }
                continue;
            }

            if (!result.has_value()) {
                result.emplace();
                result->reserve(parameters.size());
                for (std::size_t previous_index = 0; previous_index < index; ++previous_index) {
                    result->push_back(parameters[previous_index]);
                }
            }

            result->emplace_back(parameter.name, type);
        }

        return result;
    }

    std::optional<std::vector<FieldType>>
    resolve_fields_internal(const std::vector<FieldType>& fields) {
        std::optional<std::vector<FieldType>> result;

        for (std::size_t index = 0; index < fields.size(); ++index) {
            const auto& field = fields[index];
            const auto* type = resolve_type(field.type);
            if (type == field.type) {
                if (result.has_value()) {
                    result->push_back(field);
                }
                continue;
            }

            if (!result.has_value()) {
                result.emplace();
                result->reserve(fields.size());
                for (std::size_t previous_index = 0; previous_index < index; ++previous_index) {
                    result->push_back(fields[previous_index]);
                }
            }

            result->emplace_back(field.name, type, field.visibility);
        }

        return result;
    }

    struct ResolvedNominalGenerics {
        bool changed;
        std::vector<GenericParameterType> parameters;
        std::vector<GenericArgumentType> arguments;
    };

    ResolvedNominalGenerics
    resolve_nominal_generics(const std::vector<GenericParameterType>& generic_parameters,
                             const std::vector<GenericArgumentType>& generic_arguments) {
        ResolvedNominalGenerics result;
        result.changed = false;

        result.arguments =
            substitute_generic_arguments(generic_arguments, substitutions, result.changed);
        result.parameters = resolve_generic_parameters(generic_parameters, result.changed);

        for (const auto& parameter : generic_parameters) {
            if (const auto* substitution = substitutions.lookup(parameter);
                substitution != nullptr) {
                result.arguments.emplace_back("", resolve_binding(*substitution));
                result.changed = true;
            }
        }

        return result;
    }

    CompileTimeEnvironment
    build_generic_substitution_map(const std::vector<GenericParameterType>& parameters,
                                   const std::vector<GenericArgumentType>& arguments) {
        CompileTimeEnvironment substitution_map;

        for (std::size_t index = 0; index < arguments.size(); ++index) {
            substitution_map.bind(parameters[index], arguments[index].binding());
        }

        return substitution_map;
    }

    std::vector<GenericArgumentType>
    resolve_generic_arguments(const std::vector<GenericArgumentType>& arguments,
                              const std::vector<GenericParameterType>& parameters) {
        std::vector<const GenericArgumentType*> ordered(parameters.size(), nullptr);
        for (std::size_t index = 0; index < arguments.size(); ++index) {
            auto parameter_index = index;
            if (!arguments[index].name.empty()) {
                parameter_index = parameters.size();
                for (std::size_t candidate = 0; candidate < parameters.size(); ++candidate) {
                    if (parameters[candidate].name == arguments[index].name) {
                        parameter_index = candidate;
                        break;
                    }
                }
            }

            if (parameter_index >= parameters.size() || ordered[parameter_index] != nullptr) {
                diagnostics.add_error(Span(), "invalid or duplicate generic argument '" +
                                                  arguments[index].name + "'");
                return {};
            }
            ordered[parameter_index] = &arguments[index];
        }

        std::vector<GenericArgumentType> result;
        result.reserve(arguments.size());

        for (std::size_t index = 0; index < ordered.size(); ++index) {
            if (ordered[index] == nullptr) {
                diagnostics.add_error(Span(),
                                      "missing generic argument '" + parameters[index].name + "'");
                return {};
            }
            const auto& argument = *ordered[index];
            if (argument.is_const()) {
                if (!parameters[index].is_const()) {
                    diagnostics.add_error(Span(), "expected type generic argument");
                    return {};
                }
                const auto resolved = resolve_binding(argument.binding());
                const auto binding = normalize_const_binding(std::get<ConstBinding>(resolved),
                                                             parameters[index].type, Span());
                if (!binding.has_value()) {
                    return {};
                }
                result.emplace_back("", *binding);
                continue;
            }

            if (parameters[index].is_const()) {
                const auto* named =
                    argument.type != nullptr ? argument.type->as<NamedType>() : nullptr;
                auto binding = named != nullptr ? const_parameter(named->name) : std::nullopt;
                if (binding.has_value()) {
                    const auto normalized =
                        normalize_const_binding(*binding, parameters[index].type, Span());
                    if (!normalized.has_value()) {
                        return {};
                    }
                    result.emplace_back("", *normalized);
                    continue;
                }
                diagnostics.add_error(Span(), "expected const generic argument");
                return {};
            }

            result.emplace_back("", resolve_type(argument.type));
        }

        return result;
    }

    std::vector<GenericBinding>
    nominal_cache_arguments(const std::vector<GenericArgumentType>& arguments) const {
        std::vector<GenericBinding> result;
        result.reserve(arguments.size());

        for (const auto& argument : arguments) {
            result.push_back(argument.binding());
        }

        return result;
    }

    bool is_concrete_type(const Type* type, std::unordered_set<const Type*>& visited) const {
        if (type == nullptr || !visited.insert(type).second) {
            return true;
        }

        if (const auto* named = type->as<NamedType>(); named != nullptr) {
            if (substitutions.lookup(named->name) != nullptr) {
                return false;
            }

            if (lookup_type_symbol(named->name) == nullptr) {
                return false;
            }

            for (const auto& argument : named->generic_arguments) {
                if (argument.is_const()) {
                    if (!argument.const_binding->is_concrete()) {
                        return false;
                    }
                } else if (!is_concrete_type(argument.type, visited)) {
                    return false;
                }
            }

            return true;
        }

        if (const auto* pointer = type->as<PointerType>(); pointer != nullptr) {
            return is_concrete_type(pointer->element, visited);
        }

        if (const auto* array = type->as<ArrayType>(); array != nullptr) {
            return !std::holds_alternative<DependentArrayExtent>(array->extent) &&
                   is_concrete_type(array->element, visited);
        }

        if (const auto* nominal = type->as_nominal(); nominal != nullptr) {
            if (!nominal->generic_parameters.empty()) {
                return false;
            }

            for (const auto& argument : nominal->generic_arguments) {
                if (argument.is_const()) {
                    if (!argument.const_binding->is_concrete()) {
                        return false;
                    }
                } else if (!is_concrete_type(argument.type, visited)) {
                    return false;
                }
            }
        }

        if (const auto* struct_type = type->as<StructType>(); struct_type != nullptr) {
            for (const auto& field : struct_type->fields) {
                if (!is_concrete_type(field.type, visited)) {
                    return false;
                }
            }

            return true;
        }

        if (const auto* enum_type = type->as<EnumType>(); enum_type != nullptr) {
            if (!is_concrete_type(enum_type->backing_type, visited)) {
                return false;
            }

            for (const auto& variant : enum_type->variants) {
                for (const auto& field : variant.fields) {
                    if (!is_concrete_type(field.type, visited)) {
                        return false;
                    }
                }
            }

            return true;
        }

        if (const auto* interface_type = type->as<InterfaceType>(); interface_type != nullptr) {
            for (const auto* parent_interface : interface_type->interfaces) {
                if (!is_concrete_type(parent_interface, visited)) {
                    return false;
                }
            }

            return true;
        }

        if (const auto* function_type = type->as<FunctionType>(); function_type != nullptr) {
            if (!function_type->generic_parameters.empty() ||
                !is_concrete_type(function_type->return_type, visited)) {
                return false;
            }

            for (const auto& parameter : function_type->parameters) {
                if (!is_concrete_type(parameter.type, visited)) {
                    return false;
                }
            }
        }

        return true;
    }

    ArrayExtent substitute_extent(const ArrayExtent& extent,
                                  const CompileTimeEnvironment& bindings) {
        const auto* dependent = std::get_if<DependentArrayExtent>(&extent);
        if (dependent == nullptr || dependent->expression == nullptr) {
            return extent;
        }

        Evaluator evaluator(diagnostics, &env, &bindings, true);
        auto* expression =
            const_cast<Expression*>(static_cast<const Expression*>(dependent->expression));
        const auto value = evaluator.evaluate_uncached(*expression);
        const auto size = value.has_value() ? value->try_as_unsigned_integer() : std::nullopt;
        if (!size.has_value()) {
            if (value.has_value()) {
                diagnostics.add_error(expression->span,
                                      "array size must be a non-negative constant integer");
            }
            return extent;
        }

        return ConcreteArrayExtent(static_cast<std::size_t>(*size));
    }

    template <typename T>
    const Type* instantiate_named_type(const NamedType& named, const T& nominal_type) {
        if (named.generic_arguments.size() != nominal_type.generic_parameters.size()) {
            return &nominal_type;
        }

        auto arguments =
            resolve_generic_arguments(named.generic_arguments, nominal_type.generic_parameters);
        if (arguments.size() != nominal_type.generic_parameters.size()) {
            return &nominal_type;
        }

        for (const auto& argument : arguments) {
            if (argument.is_const() && !argument.const_binding->is_concrete() &&
                argument.const_binding->source_is_expression) {
                return type_arena.create<NamedType>(named.name, std::move(arguments),
                                                    named.declaration);
            }
        }

        auto substitution_map =
            build_generic_substitution_map(nominal_type.generic_parameters, arguments);
        NominalCacheKey cache_key(&nominal_type, nominal_cache_arguments(arguments));
        auto& cache = nominal_type_cache();
        if (auto iterator = cache.find(cache_key); iterator != cache.end()) {
            return iterator->second;
        }

        const auto* substituted = substitute(&nominal_type, substitution_map);
        cache.emplace(cache_key, substituted);

        const auto* resolved = resolve_type(substituted);
        const auto* resolved_nominal = resolved != nullptr ? resolved->template as<T>() : nullptr;
        if (resolved_nominal == nullptr) {
            cache.insert_or_assign(std::move(cache_key), &nominal_type);
            return &nominal_type;
        }

        const Type* result = nullptr;
        if constexpr (std::is_same_v<T, StructType>) {
            result = type_arena.create<StructType>(
                resolved_nominal->name, std::vector<GenericParameterType>(),
                resolved_nominal->fields, std::move(arguments), resolved_nominal->base_type,
                resolved_nominal->interfaces, resolved_nominal->methods, nominal_type.definition);
        } else if constexpr (std::is_same_v<T, InterfaceType>) {
            result = type_arena.create<InterfaceType>(
                resolved_nominal->name, std::vector<GenericParameterType>(),
                resolved_nominal->methods, resolved_nominal->interfaces, std::move(arguments),
                nominal_type.definition);
        } else {
            result = type_arena.create<EnumType>(
                resolved_nominal->name, std::vector<GenericParameterType>(),
                resolved_nominal->variants, resolved_nominal->backing_type, std::move(arguments),
                resolved_nominal->interfaces, resolved_nominal->methods, nominal_type.definition);
        }

        cache.insert_or_assign(std::move(cache_key), result);
        return result;
    }

  public:
    TypeResolver(TypeArena& type_arena, Env& env, Diagnostics& diagnostics)
        : type_arena(type_arena), env(env), diagnostics(diagnostics) {}

    ~TypeResolver() = default;

    class SubstitutionScope {
      private:
        TypeResolver& context;
        CompileTimeEnvironment saved;

      public:
        explicit SubstitutionScope(TypeResolver& context)
            : context(context), saved(context.substitutions) {}

        SubstitutionScope(const SubstitutionScope&) = delete;
        SubstitutionScope& operator=(const SubstitutionScope&) = delete;
        SubstitutionScope(SubstitutionScope&&) = delete;
        SubstitutionScope& operator=(SubstitutionScope&&) = delete;

        ~SubstitutionScope() {
            context.substitutions = std::move(saved);
            context.function_type_cache.clear();
        }
    };

    [[nodiscard]] SubstitutionScope create_substitution_scope() { return SubstitutionScope(*this); }

    const Type* substitute(const Type* type, const CompileTimeEnvironment& substitution_map) {
        if (type == nullptr) {
            return nullptr;
        }

        SubstitutionGuard guard(active_substitutions, SubstitutionKey(type, &substitution_map));
        if (guard.already_present) {
            return type;
        }

        switch (type->kind) {
        case Type::Kind::Type::Named: {
            const auto* named = type->as<NamedType>();
            if (const auto* binding = substitution_map.lookup_type(named->name, named->declaration);
                binding != nullptr) {
                return binding;
            }

            bool changed = false;
            auto arguments =
                substitute_generic_arguments(named->generic_arguments, substitution_map, changed);

            if (!changed) {
                return type;
            }

            return type_arena.create<NamedType>(named->name, std::move(arguments),
                                                named->declaration);
        }

        case Type::Kind::Type::Pointer: {
            const auto* pointer = type->as<PointerType>();
            const auto* element = substitute(pointer->element, substitution_map);

            if (element == pointer->element) {
                return type;
            }

            return type_arena.create<PointerType>(element, pointer->is_mutable);
        }

        case Type::Kind::Type::Array: {
            const auto* array = type->as<ArrayType>();
            const auto* element = substitute(array->element, substitution_map);
            auto extent = substitute_extent(array->extent, substitution_map);

            if (element == array->element && extent == array->extent) {
                return type;
            }

            return type_arena.create<ArrayType>(element, std::move(extent));
        }

        case Type::Kind::Type::Struct: {
            const auto* struct_type = type->as<StructType>();

            if (struct_type->generic_parameters.empty() && struct_type->generic_arguments.empty() &&
                substitution_map.empty()) {
                return type;
            }

            bool changed = false;

            auto parameters = substitute_generic_parameters(struct_type->generic_parameters,
                                                            substitution_map, changed);
            auto arguments = substitute_generic_arguments(struct_type->generic_arguments,
                                                          substitution_map, changed);
            auto fields = substitute_fields(struct_type->fields, substitution_map, changed);
            auto methods = struct_type->methods;

            const auto* base_type = substitute(struct_type->base_type, substitution_map);
            const auto* base_struct = base_type != nullptr ? base_type->as<StructType>() : nullptr;
            if (base_struct != struct_type->base_type) {
                changed = true;
            }

            std::vector<const InterfaceType*> interfaces;
            interfaces.reserve(struct_type->interfaces.size());
            for (const auto* interface_type : struct_type->interfaces) {
                const auto* substituted = substitute(interface_type, substitution_map);
                const auto* substituted_interface =
                    substituted != nullptr ? substituted->as<InterfaceType>() : nullptr;
                if (substituted_interface != interface_type) {
                    changed = true;
                }

                interfaces.push_back(substituted_interface);
            }

            if (!changed) {
                return type;
            }

            return type_arena.create<StructType>(
                struct_type->name, std::move(parameters), std::move(fields), std::move(arguments),
                base_struct, std::move(interfaces), std::move(methods), struct_type->definition);
        }

        case Type::Kind::Type::Enum: {
            const auto* enum_type = type->as<EnumType>();

            if (enum_type->generic_parameters.empty() && enum_type->generic_arguments.empty() &&
                substitution_map.empty()) {
                return type;
            }

            bool changed = false;

            auto parameters = substitute_generic_parameters(enum_type->generic_parameters,
                                                            substitution_map, changed);
            auto arguments = substitute_generic_arguments(enum_type->generic_arguments,
                                                          substitution_map, changed);

            std::vector<EnumVariantType> variants;
            variants.reserve(enum_type->variants.size());

            for (const auto& variant : enum_type->variants) {
                auto fields = substitute_fields(variant.fields, substitution_map, changed);
                variants.emplace_back(variant.name, variant.index, std::move(fields),
                                      variant.discriminant);
            }

            const auto* backing_type = substitute(enum_type->backing_type, substitution_map);
            if (backing_type != enum_type->backing_type) {
                changed = true;
            }

            std::vector<const InterfaceType*> interfaces;
            interfaces.reserve(enum_type->interfaces.size());
            for (const auto* interface_type : enum_type->interfaces) {
                const auto* substituted = substitute(interface_type, substitution_map);
                const auto* substituted_interface =
                    substituted != nullptr ? substituted->as<InterfaceType>() : nullptr;
                if (substituted_interface != interface_type) {
                    changed = true;
                }

                interfaces.push_back(substituted_interface);
            }

            auto methods = substitute_methods(enum_type->methods, substitution_map, changed);

            if (!changed) {
                return type;
            }

            return type_arena.create<EnumType>(enum_type->name, std::move(parameters),
                                               std::move(variants), backing_type,
                                               std::move(arguments), std::move(interfaces),
                                               std::move(methods), enum_type->definition);
        }

        case Type::Kind::Type::Interface: {
            const auto* interface_type = type->as<InterfaceType>();

            if (interface_type->generic_parameters.empty() &&
                interface_type->generic_arguments.empty() && substitution_map.empty()) {
                return type;
            }

            bool changed = false;

            auto parameters = substitute_generic_parameters(interface_type->generic_parameters,
                                                            substitution_map, changed);
            auto arguments = substitute_generic_arguments(interface_type->generic_arguments,
                                                          substitution_map, changed);
            auto methods = substitute_methods(interface_type->methods, substitution_map, changed);

            std::vector<const InterfaceType*> interfaces;
            interfaces.reserve(interface_type->interfaces.size());
            for (const auto* parent_interface : interface_type->interfaces) {
                const auto* substituted = substitute(parent_interface, substitution_map);
                const auto* substituted_interface =
                    substituted != nullptr ? substituted->as<InterfaceType>() : nullptr;
                if (substituted_interface != parent_interface) {
                    changed = true;
                }

                interfaces.push_back(substituted_interface);
            }

            if (!changed) {
                return type;
            }

            return type_arena.create<InterfaceType>(
                interface_type->name, std::move(parameters), std::move(methods),
                std::move(interfaces), std::move(arguments), interface_type->definition);
        }

        case Type::Kind::Type::Function: {
            const auto* function_type = type->as<FunctionType>();

            if (function_type->generic_parameters.empty() && substitution_map.empty()) {
                return type;
            }
            const auto* return_type = substitute(function_type->return_type, substitution_map);

            bool changed = return_type != function_type->return_type;

            std::vector<ParameterType> parameters;
            parameters.reserve(function_type->parameters.size());

            for (const auto& parameter : function_type->parameters) {
                const auto* parameter_type = substitute(parameter.type, substitution_map);
                if (parameter_type != parameter.type) {
                    changed = true;
                }

                parameters.emplace_back(parameter.name, parameter_type);
            }

            auto generic_parameters = substitute_generic_parameters(
                function_type->generic_parameters, substitution_map, changed);

            if (!changed) {
                return type;
            }

            return type_arena.create<FunctionType>(
                function_type->name, return_type, std::move(parameters),
                std::move(generic_parameters), function_type->variadic);
        }

        default:
            return type;
        }
    }

    const StructType* instantiate_struct(const StructType& base,
                                         const CompileTimeEnvironment& substitution_map) {
        const auto* substituted = substitute(&base, substitution_map);
        const auto* resolved = resolve_type(substituted);
        return resolved != nullptr ? resolved->as<StructType>() : nullptr;
    }

    void bind_type_parameter(const std::string& name, const Type* type,
                             const void* declaration = nullptr) {
        substitutions.bind(name, TypeBinding(type), declaration);
        function_type_cache.clear();
    }

    GenericBinding resolve_binding(const GenericBinding& binding) {
        if (const auto* type = std::get_if<TypeBinding>(&binding); type != nullptr) {
            return TypeBinding(resolve_type(type->type));
        }

        return substitute_const_binding(std::get<ConstBinding>(binding), substitutions);
    }

    std::optional<ConstBinding> normalize_const_binding(const ConstBinding& binding,
                                                        const Type* expected, Span span) {
        const auto* target = resolve_type(expected);
        if (target == nullptr || binding.type == nullptr) {
            diagnostics.add_error(span, "const generic argument type is unresolved");
            return std::nullopt;
        }
        if (binding.type->same(target)) {
            return binding;
        }
        if (!binding.type->is<IntegerType>() || !target->is<IntegerType>()) {
            diagnostics.add_error(span, "const generic argument type mismatch");
            return std::nullopt;
        }

        if (!binding.value.has_value()) {
            return ConstBinding(binding.source, target, binding.source_is_expression);
        }

        if (!binding.value->is_integer()) {
            diagnostics.add_error(span, "const generic argument type mismatch");
            return std::nullopt;
        }

        const auto numeric =
            binding.value->kind == CompileTimeValue::Kind::Type::SignedInteger
                ? static_cast<__int128>(std::get<std::int64_t>(binding.value->payload))
                : static_cast<__int128>(std::get<std::uint64_t>(binding.value->payload));
        const auto normalized =
            CompileTimeValue::checked_integer(diagnostics, numeric, target, span);
        return normalized.has_value() ? std::optional<ConstBinding>(ConstBinding(*normalized))
                                      : std::nullopt;
    }

    void bind_generic_binding(const std::string& name, const GenericBinding& binding,
                              const void* declaration = nullptr) {
        const auto resolved = resolve_binding(binding);
        substitutions.bind(name, resolved, declaration);
        function_type_cache.clear();
    }

    std::optional<ConstBinding> const_parameter(const std::string& name) {
        const auto* entry = substitutions.lookup(name);
        const auto* binding = entry != nullptr ? std::get_if<ConstBinding>(entry) : nullptr;
        return binding != nullptr
                   ? std::optional<ConstBinding>(substitute_const_binding(*binding, substitutions))
                   : std::nullopt;
    }

    const CompileTimeEnvironment& environment() const { return substitutions; }

    void use_environment(const CompileTimeEnvironment& environment) {
        substitutions = environment;
        function_type_cache.clear();
    }

    void bind_generic_parameter(const GenericParameterType& parameter, bool as_self = false) {
        if (parameter.is_const()) {
            if (parameter.declaration == nullptr) {
                diagnostics.add_error(Span(), "const parameter has no defining declaration");
                return;
            }
            substitutions.bind(parameter,
                               ConstBinding(parameter.declaration, parameter.type, false));
            function_type_cache.clear();
            return;
        }

        const auto* type =
            as_self || parameter.type == nullptr
                ? type_arena.create<NamedType>(parameter.name, std::vector<GenericArgumentType>{},
                                               parameter.declaration)
                : parameter.type;
        bind_type_parameter(parameter.name, type, parameter.declaration);
    }

    void bind_generic_parameters(const std::vector<GenericParameterType>& parameters,
                                 bool as_self = false) {
        for (const auto& parameter : parameters) {
            bind_generic_parameter(parameter, as_self);
        }
    }

    bool has_type_parameter(const std::string& name) const { return substitutions.contains(name); }

    const void* generic_parameter_declaration(const std::string& name) const {
        return substitutions.declaration(name);
    }

    bool is_concrete(const Type* type) const {
        std::unordered_set<const Type*> visited;
        return is_concrete_type(type, visited);
    }

    bool has_symbolic_bindings() const {
        for (const auto& [name, binding] : substitutions.entries()) {
            const auto* type = std::get_if<TypeBinding>(&binding);
            if (type != nullptr ? !is_concrete(type->type)
                                : !std::get<ConstBinding>(binding).is_concrete()) {
                return true;
            }
        }

        return false;
    }

    bool satisfies_constraint(const Type* argument, const Type* constraint) {
        const auto* resolved_argument = resolve_type(argument);
        const auto* resolved_constraint = resolve_type(constraint);

        if (resolved_argument == nullptr || resolved_constraint == nullptr) {
            return false;
        }

        if (resolved_argument->accepts(resolved_constraint)) {
            return true;
        }

        if (resolved_argument->is<IntegerType>() && resolved_constraint->is<IntegerType>()) {
            return true;
        }

        return false;
    }

    const FunctionType* instantiate_function_type(const FunctionType* function_type) {
        if (function_type == nullptr) {
            return nullptr;
        }

        auto cache_key = std::make_pair(function_type, env.current_scope);
        if (auto iterator = function_type_cache.find(cache_key);
            iterator != function_type_cache.end()) {
            return iterator->second;
        }

        const auto* return_type = resolve_type(function_type->return_type);

        auto changed = return_type != function_type->return_type;
        auto resolved_parameters = resolve_parameters(function_type->parameters);
        if (resolved_parameters.has_value()) {
            changed = true;
        }

        auto generic_parameters =
            resolve_generic_parameters(function_type->generic_parameters, changed);

        if (!changed) {
            function_type_cache.emplace(cache_key, function_type);
            return function_type;
        }

        auto parameters = resolved_parameters.has_value() ? std::move(*resolved_parameters)
                                                          : function_type->parameters;

        const auto* result =
            type_arena.create<FunctionType>(function_type->name, return_type, std::move(parameters),
                                            std::move(generic_parameters), function_type->variadic);
        function_type_cache.emplace(cache_key, result);
        return result;
    }

    const Type* resolve_type(const Type* type) {
        if (type == nullptr) {
            return nullptr;
        }

        ResolvingGuard guard(resolving_types, type);
        if (guard.already_present) {
            return type;
        }

        switch (type->kind) {
        case Type::Kind::Type::Named: {
            const auto* named = type->as<NamedType>();

            if (const auto* substitution =
                    substitutions.lookup_type(named->name, named->declaration);
                substitution != nullptr) {
                return substitution;
            }

            auto* symbol = lookup_type_symbol(named->name);
            if (symbol == nullptr) {
                return type;
            }

            auto* resolved_symbol_type = symbol->type;

            if (!named->generic_arguments.empty()) {
                if (const auto* struct_type = resolved_symbol_type->as<StructType>();
                    struct_type != nullptr) {
                    return instantiate_named_type(*named, *struct_type);
                }

                if (const auto* enum_type = resolved_symbol_type->as<EnumType>();
                    enum_type != nullptr) {
                    return instantiate_named_type(*named, *enum_type);
                }

                if (const auto* interface_type = resolved_symbol_type->as<InterfaceType>();
                    interface_type != nullptr) {
                    return instantiate_named_type(*named, *interface_type);
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
            auto extent = substitute_extent(array->extent, substitutions);

            if (element == array->element && extent == array->extent) {
                return type;
            }

            return type_arena.create<ArrayType>(element, std::move(extent));
        }

        case Type::Kind::Type::Struct: {
            const auto* struct_type = type->as<StructType>();

            if (struct_type->generic_parameters.empty() && struct_type->generic_arguments.empty() &&
                substitutions.entries().empty()) {
                return type;
            }

            auto [changed, parameters, arguments] = resolve_nominal_generics(
                struct_type->generic_parameters, struct_type->generic_arguments);

            auto resolved_fields = resolve_fields_internal(struct_type->fields);
            if (resolved_fields.has_value()) {
                changed = true;
            }
            auto methods = struct_type->methods;

            const auto* base_type = resolve_type(struct_type->base_type);
            const auto* base_struct = base_type != nullptr ? base_type->as<StructType>() : nullptr;
            if (base_struct != struct_type->base_type) {
                changed = true;
            }

            std::vector<const InterfaceType*> interfaces;
            interfaces.reserve(struct_type->interfaces.size());
            for (const auto* interface_type : struct_type->interfaces) {
                const auto* resolved_interface = resolve_type(interface_type);
                const auto* interface_nominal = resolved_interface != nullptr
                                                    ? resolved_interface->as<InterfaceType>()
                                                    : nullptr;
                if (interface_nominal != interface_type) {
                    changed = true;
                }

                interfaces.push_back(interface_nominal);
            }

            if (!changed) {
                return type;
            }

            auto fields =
                resolved_fields.has_value() ? std::move(*resolved_fields) : struct_type->fields;

            return type_arena.create<StructType>(
                struct_type->name, std::move(parameters), std::move(fields), std::move(arguments),
                base_struct, std::move(interfaces), std::move(methods), struct_type->definition);
        }

        case Type::Kind::Type::Enum: {
            const auto* enum_type = type->as<EnumType>();

            if (enum_type->generic_parameters.empty() && enum_type->generic_arguments.empty() &&
                substitutions.entries().empty()) {
                return type;
            }

            auto [changed, parameters, arguments] = resolve_nominal_generics(
                enum_type->generic_parameters, enum_type->generic_arguments);

            std::optional<std::vector<EnumVariantType>> resolved_variants;

            for (std::size_t index = 0; index < enum_type->variants.size(); ++index) {
                const auto& variant = enum_type->variants[index];
                auto resolved_fields = resolve_fields_internal(variant.fields);
                if (!resolved_fields.has_value()) {
                    if (resolved_variants.has_value()) {
                        resolved_variants->push_back(variant);
                    }
                    continue;
                }

                if (!resolved_variants.has_value()) {
                    resolved_variants.emplace();
                    resolved_variants->reserve(enum_type->variants.size());
                    for (std::size_t previous_index = 0; previous_index < index; ++previous_index) {
                        resolved_variants->push_back(enum_type->variants[previous_index]);
                    }
                }

                changed = true;
                resolved_variants->emplace_back(variant.name, variant.index,
                                                std::move(*resolved_fields), variant.discriminant);
            }

            const auto* backing_type = resolve_type(enum_type->backing_type);
            if (backing_type != enum_type->backing_type) {
                changed = true;
            }

            if (!changed) {
                return type;
            }

            auto variants =
                resolved_variants.has_value() ? std::move(*resolved_variants) : enum_type->variants;

            return type_arena.create<EnumType>(enum_type->name, std::move(parameters),
                                               std::move(variants), backing_type,
                                               std::move(arguments), enum_type->interfaces,
                                               enum_type->methods, enum_type->definition);
        }

        case Type::Kind::Type::Interface: {
            const auto* interface_type = type->as<InterfaceType>();

            if (interface_type->generic_parameters.empty() &&
                interface_type->generic_arguments.empty() && substitutions.entries().empty()) {
                return type;
            }

            auto [changed, parameters, arguments] = resolve_nominal_generics(
                interface_type->generic_parameters, interface_type->generic_arguments);

            std::vector<const InterfaceType*> interfaces;
            interfaces.reserve(interface_type->interfaces.size());
            for (const auto* parent_interface : interface_type->interfaces) {
                const auto* resolved_interface = resolve_type(parent_interface);
                const auto* interface_nominal = resolved_interface != nullptr
                                                    ? resolved_interface->as<InterfaceType>()
                                                    : nullptr;
                if (interface_nominal != parent_interface) {
                    changed = true;
                }

                interfaces.push_back(interface_nominal);
            }

            if (!changed) {
                return type;
            }

            auto methods = interface_type->methods;

            return type_arena.create<InterfaceType>(
                interface_type->name, std::move(parameters), std::move(methods),
                std::move(interfaces), std::move(arguments), interface_type->definition);
        }

        case Type::Kind::Type::Function: {
            const auto* function_type = type->as<FunctionType>();

            return instantiate_function_type(function_type);
        }

        default:
            return type;
        }
    }
};
