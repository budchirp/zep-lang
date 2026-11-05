module;

#include <string>
#include <utility>
#include <variant>
#include <vector>

module zep.compiler.lowering;
import zep.frontend.node;
import zep.frontend.kind;
import zep.frontend.sema.types;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.compiler.lowering.mangler;
import zep.hir.node;

std::string Lowerer::type_name(const NominalType& type) const {
    const auto* symbol = sema.env.root_scope->lookup_type(type.name);

    return Mangler::linker_name(type.name, symbol);
}

const Type* Lowerer::lower_type(const Type* type, Span span) {
    if (type == nullptr) {
        return nullptr;
    }

    const auto* nominal = type->as_nominal();
    const auto* resolved = nominal != nullptr && resolver.is_concrete(nominal)
        ? type : resolver.resolve_type(type);

    if (resolved == nullptr) {
        return nullptr;
    }

    if (const auto cached = lower_type_cache.find(resolved); cached != lower_type_cache.end()) {
        return cached->second;
    }

    const Type* lowered = resolved;

    if (const auto* backing = facades.backing(resolved); backing != nullptr) {
        lowered = lower_type(backing, span);
    } else if (const auto* pointer = resolved->as<PointerType>(); pointer != nullptr) {
        lowered = sema.types.create<PointerType>(
            lower_type(pointer->element, span), pointer->is_mutable, pointer->reference_kind);
    } else if (const auto* array = resolved->as<ArrayType>(); array != nullptr) {
        lowered = sema.types.create<ArrayType>(lower_type(array->element, span), array->extent);
    } else if (const auto* function = resolved->as<FunctionType>(); function != nullptr) {
        std::vector<ParameterType> parameters;
        parameters.reserve(function->parameters.size());

        for (const auto& parameter : function->parameters) {
            parameters.emplace_back(parameter.name, lower_type(parameter.type, span));
        }

        lowered = sema.types.create<FunctionType>(function->name,
            lower_type(function->return_type, span), std::move(parameters),
            std::vector<GenericParameterType>(), function->variadic);
    } else if (const auto* structure = resolved->as<StructType>(); structure != nullptr) {
        lowered = lower_struct_type(type, structure, span);
    } else if (const auto* enumeration = resolved->as<EnumType>(); enumeration != nullptr) {
        lowered = lower_enum_type(enumeration, span);
    } else if (const auto* interface = resolved->as<InterfaceType>(); interface != nullptr) {
        lowered = lower_interface_type(interface, span);
    }

    lower_type_cache.emplace(resolved, lowered);

    return lowered;
}

const Type* Lowerer::lower_enum_type(const EnumType* type, Span span) {
    if (!type->generic_parameters.empty() || !resolver.is_concrete(type)) {
        return type;
    }

    std::vector<EnumVariantType> variants;
    variants.reserve(type->variants.size());

    const auto* backing = lower_type(type->backing_type, span);
    auto changed = backing != type->backing_type;

    for (const auto& variant : type->variants) {
        std::vector<FieldType> fields;
        fields.reserve(variant.fields.size());

        for (const auto& field : variant.fields) {
            const auto* lowered = lower_type(field.type, span);
            changed = changed || lowered != field.type;
            fields.emplace_back(field.name, lowered);
        }

        variants.emplace_back(variant.name, variant.index, std::move(fields), variant.discriminant);
    }

    const auto* lowered = changed
        ? sema.types.create<EnumType>(type->name, std::vector<GenericParameterType>(),
            std::move(variants), backing, type->generic_arguments, type->interfaces,
            type->methods, type->definition)
        : type;

    register_type_symbol(type_name(*type), Visibility::Type::Public, lowered, span);

    return lowered;
}

const Type* Lowerer::lower_interface_type(const InterfaceType* type, Span span) {
    if (!type->generic_parameters.empty() || !resolver.is_concrete(type)) {
        return type;
    }

    std::vector<MethodType> methods;
    methods.reserve(type->methods.size());

    for (const auto& method : type->methods) {
        methods.emplace_back(method.name, lower_function_type(method.type, span), method.index);
    }

    auto* lowered = sema.types.create<InterfaceType>(type->name,
        std::vector<GenericParameterType>(), std::move(methods), type->interfaces,
        type->generic_arguments, type->definition);

    register_type_symbol(type_name(*type), Visibility::Type::Public, lowered, span);

    return lowered;
}

const Type* Lowerer::lower_struct_type(const Type* source, const StructType* type, Span span) {
    if (type->generic_parameters.empty() && !type->generic_arguments.empty() &&
        type->definition != type && type->name != type->definition->name) {
        return type;
    }

    const auto* definition = specializations.get_struct(type->name);
    std::vector<GenericBinding> bindings;

    if (const auto* named = source->as<NamedType>();
        named != nullptr && !named->generic_arguments.empty()) {
        bindings = lower_generic_arguments(named->generic_arguments, span);
    } else if (!type->generic_arguments.empty()) {
        bindings = lower_generic_arguments(type->generic_arguments, span);
    } else if (definition != nullptr && !definition->generic_parameters.empty()) {
        bindings.reserve(definition->generic_parameters.size());

        for (const auto* parameter : definition->generic_parameters) {
            const auto constant = resolver.const_parameter(parameter->name);

            if (parameter->is_const() && constant.has_value()) {
                bindings.emplace_back(*constant);
            } else {
                const auto* named = sema.types.create<NamedType>(
                    parameter->name, std::vector<GenericArgumentType>(), parameter);
                bindings.emplace_back(TypeBinding(resolver.resolve_type(named)));
            }
        }
    }

    if (bindings.empty()) {
        register_type_symbol(type_name(*type), Visibility::Type::Public, type, span);

        return type;
    }

    for (const auto& binding : bindings) {
        if (!is_concrete_binding(binding)) {
            return type;
        }
    }

    const auto identity = definition != nullptr ? std::variant<const Node*, const Type*>(definition)
        : std::variant<const Node*, const Type*>(type);
    const auto instance = specializations.get_or_create(identity, type->name, bindings);

    if (const auto* existing = sema.env.root_scope->lookup_type(instance.name); existing != nullptr) {
        return existing->type;
    }

    std::vector<GenericArgumentType> arguments;
    arguments.reserve(bindings.size());

    for (auto index = std::size_t{0}; index < bindings.size(); ++index) {
        const auto name = definition != nullptr && index < definition->generic_parameters.size()
            ? definition->generic_parameters[index]->name : std::string();

        arguments.emplace_back(name, bindings[index]);
    }

    auto* structure = sema.types.create<StructType>(instance.name,
        std::vector<GenericParameterType>(), std::vector<FieldType>(), std::move(arguments),
        nullptr, std::vector<const InterfaceType*>(), std::vector<MethodType>(), type->definition);

    register_type_symbol(instance.name, Visibility::Type::Public, structure, span);
    auto scope = resolver.create_substitution_scope();

    if (definition != nullptr) {
        apply_substitutions(definition->generic_parameters, bindings);
    }

    structure->base_type = type->base_type != nullptr
        ? lower_type(type->base_type, span)->as<StructType>() : nullptr;
    structure->interfaces.reserve(type->interfaces.size());

    for (const auto* interface : type->interfaces) {
        const auto* lowered = lower_type(interface, span);
        structure->interfaces.push_back(lowered != nullptr ? lowered->as<InterfaceType>() : nullptr);
    }

    structure->fields.reserve(type->fields.size());

    for (const auto& field : type->fields) {
        structure->fields.emplace_back(field.name, lower_type(field.type, span), field.visibility);
    }

    structure->methods = type->methods;

    if (definition == nullptr) {
        return structure;
    }

    for (const auto* method : definition->methods) {
        auto required = method->is_override;

        for (const auto* interface : structure->interfaces) {
            if (interface == nullptr) {
                continue;
            }

            for (const auto& entry : interface->methods) {
                required = required || entry.name == method->prototype->name;
            }
        }

        if (!required || method->is_deleted) {
            continue;
        }

        const auto name = method->function_symbol != nullptr
            ? method->function_symbol->base_name() : method->prototype->name;
        auto method_scope = resolver.create_substitution_scope();
        apply_parent_substitutions(definition->name, bindings);

        if (lower_function_type(method->type, method->span) == nullptr) {
            continue;
        }

        const auto instance = specializations.get_or_create(method, name, bindings);

        if (instance.is_generated) {
            continue;
        }

        auto* specialization = specialize(*method, instance.name, bindings);

        if (specialization != nullptr) {
            specializations.enqueue(specialization);
        }
    }

    return structure;
}
