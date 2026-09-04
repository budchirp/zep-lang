module;

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

export module zep.compiler.lowering;

import zep.common.source.span;
import zep.common.context;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.sema.context;
import zep.common.diagnostic.diagnostic;
import zep.common.diagnostic.collection;
import zep.frontend.sema.constant.evaluator;
import zep.frontend.sema.env;
import zep.frontend.sema.kind;
import zep.frontend.sema.resolver.attribute;
import zep.frontend.sema.resolver.facade;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.const_size;
import zep.frontend.sema.type.resolver;
import zep.frontend.sema.checker;
import zep.compiler.cleanup;
import zep.compiler.lowering.mangler;
import zep.compiler.lowering.specialization;
import zep.hir.node;
import zep.hir.program;

export class HIRLowerer : public Visitor<HIRNode*> {
  private:
    class LoweringFailure {
      public:
        Span span;
        std::string message;

        LoweringFailure(Span span, std::string message) : span(span), message(std::move(message)) {}
    };

    SemaContext& sema;
    Context& context;
    std::shared_ptr<HIRProgram> program;

    TypeResolver resolver;
    FacadeResolver facades;

    HIRCleanup cleanup;
    MonomorphizationCache mono_cache;

    std::unordered_map<const Type*, const Type*> lower_type_cache;
    std::unordered_map<const VariableSymbol*, std::pair<std::string, const PointerType*>>
        captured_variables;
    bool lowering_monomorphized_body = false;
    const FunctionSymbol* current_function = nullptr;
    std::string current_parent;

    HIRExpression* lower_expression(Expression& node) {
        return static_cast<HIRExpression*>(visit_expression(node));
    }

    HIRStatement* lower_statement(Statement& node) {
        return static_cast<HIRStatement*>(visit_statement(node));
    }

    HIRBlockStatement* lower_block(Statement& node) {
        return static_cast<HIRBlockStatement*>(lower_statement(node));
    }

    const Type* lower_type(const Type* type, Span span) {
        if (type == nullptr) {
            return nullptr;
        }

        const auto* nominal = type->as_nominal();
        const auto* resolved = nominal != nullptr && resolver.is_concrete(nominal)
                                   ? type
                                   : resolver.resolve_type(type);

        if (auto cached = lower_type_cache.find(resolved); cached != lower_type_cache.end()) {
            return cached->second;
        }

        if (const auto* backing_type = facades.resolve_backing(resolved); backing_type != nullptr) {
            const auto* lowered = lower_type(backing_type, span);
            lower_type_cache.emplace(resolved, lowered);
            return lowered;
        }

        if (const auto* pointer_type = resolved->as<PointerType>(); pointer_type != nullptr) {
            auto* lowered = sema.types.create<PointerType>(lower_type(pointer_type->element, span),
                                                           pointer_type->is_mutable);
            lower_type_cache.emplace(resolved, lowered);
            return lowered;
        }

        if (const auto* array_type = resolved->as<ArrayType>(); array_type != nullptr) {
            const auto* element = lower_type(array_type->element, span);
            const auto* lowered = sema.types.create<ArrayType>(element, array_type->extent);
            lower_type_cache.emplace(resolved, lowered);
            return lowered;
        }

        if (const auto* function_type = resolved->as<FunctionType>(); function_type != nullptr) {
            std::vector<ParameterType> parameter_types;
            parameter_types.reserve(function_type->parameters.size());

            for (const auto& parameter : function_type->parameters) {
                parameter_types.emplace_back(parameter.name, lower_type(parameter.type, span));
            }

            auto* lowered = sema.types.create<FunctionType>(
                function_type->name, lower_type(function_type->return_type, span),
                std::move(parameter_types), std::vector<GenericParameterType>(),
                function_type->variadic);
            lower_type_cache.emplace(resolved, lowered);
            return lowered;
        }

        if (const auto* struct_type = resolved->as<StructType>(); struct_type != nullptr) {
            auto* lowered = lower_struct_type(type, struct_type, span);
            lower_type_cache.emplace(resolved, lowered);
            return lowered;
        }

        if (const auto* enum_type = resolved->as<EnumType>(); enum_type != nullptr) {
            auto* lowered = lower_enum_type(enum_type, span);
            lower_type_cache.emplace(resolved, lowered);
            return lowered;
        }

        if (const auto* interface_type = resolved->as<InterfaceType>(); interface_type != nullptr) {
            auto* lowered = lower_interface_type(interface_type, span);
            lower_type_cache.emplace(resolved, lowered);
            return lowered;
        }

        lower_type_cache.emplace(resolved, resolved);
        return resolved;
    }

    const FunctionType* lower_function_type(const Type* type, Span span) {
        const auto* lowered = lower_type(type, span);

        return lowered != nullptr ? lowered->as<FunctionType>() : nullptr;
    }

    static std::size_t parameter_offset(const FunctionType* function_type,
                                        std::size_t declared_count) {
        if (function_type == nullptr || function_type->parameters.size() <= declared_count) {
            return 0;
        }

        return function_type->parameters.size() - declared_count;
    }

    bool terminates(HIRStatement* statement) const {
        if (statement == nullptr) {
            return false;
        }

        if (statement->as<HIRReturnStatement>() != nullptr) {
            return true;
        }

        if (auto* group = statement->as<HIRStatementGroup>(); group != nullptr) {
            if (group->statements.empty()) {
                return false;
            }

            return terminates(group->statements.back());
        }

        auto* block = statement->as<HIRBlockStatement>();
        if (block == nullptr || block->statements.empty()) {
            return false;
        }

        return terminates(block->statements.back());
    }

    bool is_emittable_type(const Type* type, std::unordered_set<const Type*>& visited) {
        if (type == nullptr || !visited.insert(type).second) {
            return true;
        }

        if (const auto* named_type = type->as<NamedType>(); named_type != nullptr) {
            for (const auto& argument : named_type->generic_arguments) {
                if (argument.is_const()) {
                    if (!argument.const_binding->is_concrete()) {
                        return false;
                    }
                } else if (!is_emittable_type(argument.type, visited)) {
                    return false;
                }
            }

            if (named_type->name == "Self") {
                return true;
            }
        }

        const auto* resolved = resolver.resolve_type(type);
        if (resolved != nullptr && resolved != type) {
            return is_emittable_type(resolved, visited);
        }

        if (const auto* named_type = type->as<NamedType>(); named_type != nullptr) {
            return false;
        }

        if (const auto* pointer_type = type->as<PointerType>(); pointer_type != nullptr) {
            return is_emittable_type(pointer_type->element, visited);
        }

        if (const auto* array_type = type->as<ArrayType>(); array_type != nullptr) {
            return !std::holds_alternative<DependentArrayExtent>(array_type->extent) &&
                   is_emittable_type(array_type->element, visited);
        }

        if (const auto* function_type = type->as<FunctionType>(); function_type != nullptr) {
            return function_type->generic_parameters.empty();
        }

        if (const auto* nominal_type = type->as_nominal(); nominal_type != nullptr) {
            if (!nominal_type->generic_parameters.empty()) {
                return false;
            }

            for (const auto& argument : nominal_type->generic_arguments) {
                if (argument.is_const()) {
                    if (!argument.const_binding->is_concrete()) {
                        return false;
                    }
                } else if (!is_emittable_type(argument.type, visited)) {
                    return false;
                }
            }
        }

        if (const auto* struct_type = type->as<StructType>(); struct_type != nullptr) {
            for (const auto& field : struct_type->fields) {
                if (!is_emittable_type(field.type, visited)) {
                    return false;
                }
            }

            return true;
        }

        if (const auto* enum_type = type->as<EnumType>(); enum_type != nullptr) {
            if (!is_emittable_type(enum_type->backing_type, visited)) {
                return false;
            }

            for (const auto& variant : enum_type->variants) {
                for (const auto& field : variant.fields) {
                    if (!is_emittable_type(field.type, visited)) {
                        return false;
                    }
                }
            }

            return true;
        }

        if (const auto* interface_type = type->as<InterfaceType>(); interface_type != nullptr) {
            for (const auto* parent_interface : interface_type->interfaces) {
                if (!is_emittable_type(parent_interface, visited)) {
                    return false;
                }
            }
        }

        return true;
    }

    bool is_emittable_function_type(const FunctionType* function_type) {
        if (function_type == nullptr || !function_type->generic_parameters.empty()) {
            return false;
        }

        std::unordered_set<const Type*> visited;
        if (!is_emittable_type(function_type->return_type, visited)) {
            return false;
        }

        for (const auto& parameter : function_type->parameters) {
            if (!is_emittable_type(parameter.type, visited)) {
                return false;
            }
        }

        return true;
    }

    void bind_parameter(const GenericParameterType& parameter, const GenericBinding& binding) {
        resolver.bind_generic_binding(parameter.name, binding, parameter.declaration);
    }

    bool is_concrete_binding(const GenericBinding& binding) const {
        const auto* type = std::get_if<TypeBinding>(&binding);
        if (type != nullptr) {
            return resolver.is_concrete(type->type);
        }

        return std::get<ConstBinding>(binding).value.has_value();
    }

    void apply_substitutions(const std::vector<GenericParameter*>& parameters,
                             const std::vector<GenericBinding>& arguments) {
        const auto count = std::min(parameters.size(), arguments.size());

        for (std::size_t index = 0; index < count; ++index) {
            resolver.bind_generic_binding(parameters[index]->name, arguments[index],
                                          parameters[index]);
        }
    }

    void apply_parent_substitutions(const std::string& parent,
                                    const std::vector<GenericBinding>& arguments) {
        if (parent.empty() || arguments.empty()) {
            return;
        }

        const auto* parent_symbol = sema.env.current_scope->lookup_type(parent);
        const auto* nominal =
            parent_symbol != nullptr ? parent_symbol->type->as_nominal() : nullptr;
        if (nominal == nullptr) {
            return;
        }

        const auto count = std::min(nominal->generic_parameters.size(), arguments.size());
        for (std::size_t index = 0; index < count; ++index) {
            bind_parameter(nominal->generic_parameters[index], arguments[index]);
        }
    }

    void apply_parent_substitutions(const FunctionSymbol& symbol, const Type* receiver_type) {
        if (symbol.parent.empty() || receiver_type == nullptr) {
            return;
        }

        const auto* parent_symbol = sema.env.current_scope->lookup_type(symbol.parent);
        const auto* definition =
            parent_symbol != nullptr ? parent_symbol->type->as_nominal() : nullptr;
        const auto* nominal = receiver_type->as_nominal();
        if (definition == nullptr || nominal == nullptr) {
            return;
        }

        const auto count =
            std::min(definition->generic_parameters.size(), nominal->generic_arguments.size());
        for (std::size_t index = 0; index < count; ++index) {
            bind_parameter(definition->generic_parameters[index],
                           nominal->generic_arguments[index].binding());
        }
    }

    std::vector<GenericBinding>
    lower_generic_arguments(const std::vector<GenericArgumentType>& arguments, Span span) {
        std::vector<GenericBinding> bindings;
        bindings.reserve(arguments.size());

        for (const auto& argument : arguments) {
            if (argument.is_const()) {
                bindings.push_back(resolver.resolve_binding(argument.binding()));
            } else {
                bindings.emplace_back(TypeBinding(lower_type(argument.type, span)));
            }
        }

        return bindings;
    }

    std::vector<GenericBinding>
    lower_generic_arguments(const std::vector<GenericArgument*>& arguments, Span span) {
        std::vector<GenericBinding> bindings;
        bindings.reserve(arguments.size());

        for (const auto* argument : arguments) {
            if (argument->const_binding.has_value()) {
                bindings.push_back(resolver.resolve_binding(*argument->const_binding));
            } else {
                bindings.emplace_back(TypeBinding(lower_type(argument->type->type, span)));
            }
        }

        return bindings;
    }

    const Type* lower_enum_type(const EnumType* enum_type, Span span) {
        if (!enum_type->generic_parameters.empty() || !resolver.is_concrete(enum_type)) {
            return enum_type;
        }

        std::vector<EnumVariantType> variants;
        variants.reserve(enum_type->variants.size());

        auto changed = false;
        const auto* backing_type = lower_type(enum_type->backing_type, span);
        if (backing_type != enum_type->backing_type) {
            changed = true;
        }

        for (const auto& variant : enum_type->variants) {
            std::vector<FieldType> fields;
            fields.reserve(variant.fields.size());

            for (const auto& field : variant.fields) {
                const auto* field_type = lower_type(field.type, span);
                if (field_type != field.type) {
                    changed = true;
                }

                fields.emplace_back(field.name, field_type);
            }

            variants.emplace_back(variant.name, variant.index, std::move(fields),
                                  variant.discriminant);
        }

        const auto* lowered =
            changed
                ? sema.types.create<EnumType>(enum_type->name, std::vector<GenericParameterType>(),
                                              std::move(variants), backing_type,
                                              enum_type->generic_arguments, enum_type->interfaces,
                                              enum_type->methods, enum_type->definition)
                : enum_type;

        auto register_name = lowered->name;
        if (const auto* type_symbol = sema.env.root_scope->lookup_type(enum_type->name);
            type_symbol != nullptr) {
            register_name = Mangler::linker_name(enum_type->name, type_symbol);
        }

        register_type_symbol(register_name, Visibility::Type::Public, lowered, span);

        return lowered;
    }

    std::vector<MethodType> lower_methods(const std::vector<MethodType>& methods, Span span) {
        std::vector<MethodType> result;
        result.reserve(methods.size());

        for (const auto& method : methods) {
            const auto* type = lower_function_type(method.type, span);
            result.emplace_back(method.name, type, method.index);
        }

        return result;
    }

    const Type* lower_interface_type(const InterfaceType* interface_type, Span span) {
        if (!interface_type->generic_parameters.empty() || !resolver.is_concrete(interface_type)) {
            return interface_type;
        }

        auto methods = lower_methods(interface_type->methods, span);
        auto* lowered = sema.types.create<InterfaceType>(
            interface_type->name, std::vector<GenericParameterType>(), std::move(methods),
            interface_type->interfaces, interface_type->generic_arguments,
            interface_type->definition);

        auto register_name = lowered->name;
        if (const auto* type_symbol = sema.env.root_scope->lookup_type(interface_type->name);
            type_symbol != nullptr) {
            register_name = Mangler::linker_name(interface_type->name, type_symbol);
        }

        register_type_symbol(register_name, Visibility::Type::Public, lowered, span);

        return lowered;
    }

    const Type* lower_struct_type(const Type* type, const StructType* struct_type, Span span) {
        if (struct_type->generic_parameters.empty() && !struct_type->generic_arguments.empty() &&
            struct_type->definition != struct_type &&
            struct_type->name != struct_type->definition->name) {
            return struct_type;
        }

        std::vector<GenericBinding> argument_types;

        const auto* definition = mono_cache.get_struct(struct_type->name);

        if (const auto* named_type = type->as<NamedType>();
            named_type != nullptr && !named_type->generic_arguments.empty()) {
            argument_types = lower_generic_arguments(named_type->generic_arguments, span);
        } else if (!struct_type->generic_arguments.empty()) {
            argument_types = lower_generic_arguments(struct_type->generic_arguments, span);
        } else if (definition != nullptr && !definition->generic_parameters.empty()) {
            argument_types.reserve(definition->generic_parameters.size());

            for (const auto& parameter : definition->generic_parameters) {
                const auto constant = resolver.const_parameter(parameter->name);
                if (parameter->is_const() && constant.has_value()) {
                    argument_types.emplace_back(*constant);
                } else {
                    argument_types.emplace_back(
                        TypeBinding(resolver.resolve_type(sema.types.create<NamedType>(
                            parameter->name, std::vector<GenericArgumentType>(), parameter))));
                }
            }
        }

        if (argument_types.empty()) {
            auto register_name = struct_type->name;

            if (const auto* type_symbol = sema.env.root_scope->lookup_type(struct_type->name);
                type_symbol != nullptr) {
                register_name = Mangler::linker_name(struct_type->name, type_symbol);
            }

            register_type_symbol(register_name, Visibility::Type::Public, struct_type, span);

            return struct_type;
        }

        for (const auto& argument_type : argument_types) {
            if (!is_concrete_binding(argument_type)) {
                return struct_type;
            }
        }

        const auto identity = definition != nullptr
                                  ? std::variant<const Node*, const Type*>(definition)
                                  : std::variant<const Node*, const Type*>(struct_type);
        const auto instance = mono_cache.get_or_create(identity, struct_type->name, argument_types);

        if (const auto* prior_symbol = sema.env.root_scope->lookup_type(instance.name);
            prior_symbol != nullptr) {
            return prior_symbol->type;
        }

        std::vector<GenericArgumentType> instance_arguments;
        instance_arguments.reserve(argument_types.size());
        for (std::size_t index = 0; index < argument_types.size(); ++index) {
            const auto name = definition != nullptr && index < definition->generic_parameters.size()
                                  ? definition->generic_parameters[index]->name
                                  : std::string();
            instance_arguments.emplace_back(name, argument_types[index]);
        }

        auto* instance_struct = sema.types.create<StructType>(
            instance.name, std::vector<GenericParameterType>(), std::vector<FieldType>(),
            std::move(instance_arguments), nullptr, std::vector<const InterfaceType*>(),
            std::vector<MethodType>(), struct_type->definition);

        register_type_symbol(instance.name, Visibility::Type::Public, instance_struct, span);

        auto scope = resolver.create_substitution_scope();

        if (definition != nullptr) {
            apply_substitutions(definition->generic_parameters, argument_types);
        }

        instance_struct->base_type =
            struct_type->base_type != nullptr
                ? lower_type(struct_type->base_type, span)->as<StructType>()
                : nullptr;
        instance_struct->interfaces.reserve(struct_type->interfaces.size());
        for (const auto* interface_type : struct_type->interfaces) {
            const auto* lowered_interface = lower_type(interface_type, span);
            instance_struct->interfaces.push_back(
                lowered_interface != nullptr ? lowered_interface->as<InterfaceType>() : nullptr);
        }
        instance_struct->fields.reserve(struct_type->fields.size());

        for (const auto& field : struct_type->fields) {
            instance_struct->fields.emplace_back(field.name, lower_type(field.type, span),
                                                 field.visibility);
        }

        instance_struct->methods = struct_type->methods;

        if (definition != nullptr) {
            for (auto* method : definition->methods) {
                auto base_name = method->function_symbol != nullptr
                                     ? method->function_symbol->base_name()
                                     : method->prototype->name;

                auto method_scope = resolver.create_substitution_scope();
                apply_parent_substitutions(definition->name, argument_types);

                const auto* lowered_method_type =
                    lower_type(method->type, method->span)->as<FunctionType>();
                if (lowered_method_type == nullptr) {
                    continue;
                }

                auto method_name =
                    Mangler::function_name(base_name, method->function_symbol, lowered_method_type);

                if (!mono_cache.mark_specialization(method, argument_types, method_name)) {
                    continue;
                }

                auto* specialization =
                    lower_monomorphized_function(*method, method_name, argument_types);
                if (specialization != nullptr) {
                    mono_cache.enqueue_specialization(specialization);
                }
            }
        }

        return instance_struct;
    }

    HIRFunctionDeclaration*
    lower_monomorphized_function(const FunctionDeclaration& declaration,
                                 const std::string& mangled_name,
                                 const std::vector<GenericBinding>& argument_types) {
        auto scope = resolver.create_substitution_scope();

        const auto* declared_function_type =
            declaration.type != nullptr ? declaration.type->as<FunctionType>() : nullptr;
        if (declared_function_type != nullptr) {
            const auto count =
                std::min(declared_function_type->generic_parameters.size(), argument_types.size());

            for (std::size_t index = 0; index < count; ++index) {
                bind_parameter(declared_function_type->generic_parameters[index],
                               argument_types[index]);
            }
        }

        const auto variadic = declaration.prototype->is_variadic;
        const auto* return_type =
            lower_type(declaration.prototype->return_type->type, declaration.span);

        const auto* type_base = lower_type(declaration.type, declaration.span);
        const auto* function_type = type_base != nullptr ? type_base->as<FunctionType>() : nullptr;

        if (function_type == nullptr) {
            return nullptr;
        }

        std::vector<ParameterType> parameters;
        parameters.reserve(function_type->parameters.size());

        for (const auto& parameter : function_type->parameters) {
            parameters.emplace_back(parameter.name, lower_type(parameter.type, declaration.span));
        }

        const auto* mangled_type =
            sema.types.create<FunctionType>(mangled_name, return_type, std::move(parameters),
                                            std::vector<GenericParameterType>(), variadic);
        if (!is_emittable_function_type(mangled_type)) {
            return nullptr;
        }

        auto mangled = declaration.is_mangled();
        ScopeGuard function_scope(sema.env.current_scope, sema.env.scopes,
                                  Scope::Kind::Type::Function, declaration.prototype->name);

        auto offset = parameter_offset(mangled_type, declaration.prototype->parameters.size());

        if (offset > 0) {
            const auto& self = mangled_type->parameters.front();
            sema.env.current_scope->define_var(
                self.name, sema.env.symbols.create<VariableSymbol>(
                               self.name, declaration.prototype->span, Visibility::Type::Private,
                               StorageKind::Type::Var, self.type));
        }

        for (std::size_t index = 0; index < declaration.prototype->parameters.size(); ++index) {
            auto* parameter = declaration.prototype->parameters[index];
            const auto parameter_index = index + offset;
            const auto* parameter_type = parameter_index < mangled_type->parameters.size()
                                             ? mangled_type->parameters[parameter_index].type
                                             : nullptr;

            sema.env.current_scope->define_var(
                parameter->name, sema.env.symbols.create<VariableSymbol>(
                                     parameter->name, parameter->span, Visibility::Type::Private,
                                     StorageKind::Type::Var, parameter_type));
        }

        auto previous_lowering_monomorphized_body = lowering_monomorphized_body;
        const auto* previous_function = current_function;
        auto previous_parent = current_parent;
        lowering_monomorphized_body = true;
        current_function = declaration.function_symbol;
        current_parent = declaration.parent;
        auto* body = lower_block(*declaration.body);
        lowering_monomorphized_body = previous_lowering_monomorphized_body;
        current_function = previous_function;
        current_parent = std::move(previous_parent);

        return program->context.nodes.create<HIRFunctionDeclaration>(
            declaration.span, declaration.visibility, Linkage::Type::LinkOnceODR, mangled_name,
            return_type, body, variadic, mangled_type, mangled, Abi::Type::Language);
    }

    HIRExpression* lower_compile_time_value(Span span, CompileTimeValue value) {
        const auto* type = lower_type(value.type, span);

        switch (value.kind) {
        case CompileTimeValue::Kind::Type::SignedInteger:
            return program->context.nodes.create<HIRNumberLiteral>(
                span, std::to_string(std::get<std::int64_t>(value.payload)), type);

        case CompileTimeValue::Kind::Type::UnsignedInteger:
            return program->context.nodes.create<HIRNumberLiteral>(
                span, std::to_string(std::get<std::uint64_t>(value.payload)), type);

        case CompileTimeValue::Kind::Type::Float:
            return program->context.nodes.create<HIRFloatLiteral>(span, value.to_string(), type);

        case CompileTimeValue::Kind::Type::Boolean:
            return program->context.nodes.create<HIRBooleanLiteral>(
                span, std::get<bool>(value.payload), type);

        case CompileTimeValue::Kind::Type::Char:
            return program->context.nodes.create<HIRCharLiteral>(
                span, std::get<std::uint8_t>(value.payload), type);

        case CompileTimeValue::Kind::Type::String:
            return program->context.nodes.create<HIRStringLiteral>(
                span, std::get<std::string>(value.payload), type);

        case CompileTimeValue::Kind::Type::Null:
            return program->context.nodes.create<HIRNullLiteral>(span, type);

        case CompileTimeValue::Kind::Type::Struct:
            return nullptr;
        case CompileTimeValue::Kind::Type::Array: {
            const auto& elements = std::get<std::vector<CompileTimeValue>>(value.payload);
            const auto* array = type != nullptr ? type->as<ArrayType>() : nullptr;
            if (array == nullptr || array->element == nullptr) {
                throw LoweringFailure(span, "constant array requires a concrete element type");
            }
            if (!array->element->is<StringType>()) {
                std::vector<HIRExpression*> expressions;
                expressions.reserve(elements.size());
                for (const auto& element : elements) {
                    auto* expression = lower_compile_time_value(span, element);
                    if (expression == nullptr) {
                        throw LoweringFailure(
                            span, "constant array element cannot be represented at runtime");
                    }
                    expressions.push_back(expression);
                }
                return program->context.nodes.create<HIRArrayLiteralExpression>(
                    span, std::move(expressions), type);
            }
            std::vector<std::string> strings;
            strings.reserve(elements.size());
            for (const auto& element : elements) {
                if (element.kind != CompileTimeValue::Kind::Type::String) {
                    throw LoweringFailure(
                        span, "constant array element cannot be represented at runtime");
                }
                strings.push_back(std::get<std::string>(element.payload));
            }
            return program->context.nodes.create<HIRStringArrayExpression>(span, std::move(strings),
                                                                           type);
        }
        }

        return nullptr;
    }

    HIRExpression* lower_facade_literal(StructLiteralExpression& node) {
        auto* value = facades.extract_literal_value(node);
        if (value == nullptr) {
            return nullptr;
        }

        return lower_expression(*value);
    }

    std::vector<HIRStructLiteralField>
    lower_literal_fields(std::vector<StructLiteralField*>& fields) {
        std::vector<HIRStructLiteralField> result;
        result.reserve(fields.size());

        for (auto* field : fields) {
            result.emplace_back(field->name, lower_expression(*field->value));
        }

        return result;
    }

    void define_parameters(FunctionPrototype& prototype, const FunctionType* function_type) {
        auto offset = parameter_offset(function_type, prototype.parameters.size());
        if (offset > 0) {
            const auto& self = function_type->parameters.front();
            sema.env.current_scope->define_var(self.name, sema.env.symbols.create<VariableSymbol>(
                                                              self.name, prototype.span,
                                                              Visibility::Type::Private,
                                                              StorageKind::Type::Var, self.type));
        }

        for (auto* parameter : prototype.parameters) {
            const auto* parameter_type = resolver.resolve_type(parameter->type->type);
            sema.env.current_scope->define_var(
                parameter->name, sema.env.symbols.create<VariableSymbol>(
                                     parameter->name, prototype.span, Visibility::Type::Private,
                                     StorageKind::Type::Var, parameter_type));
        }
    }

    HIRBlockStatement* build_owned_body(const std::vector<Parameter*>& source_parameters,
                                        const FunctionType* function_type, BlockStatement& body,
                                        std::size_t hidden_parameter_count = 0) {
        CleanupFunctionGuard function_guard(cleanup);
        CleanupScopeGuard scope_guard(cleanup);

        std::vector<HIRStatement*> parameter_flags;
        parameter_flags.reserve(source_parameters.size());

        auto offset = parameter_offset(function_type, source_parameters.size());
        if (offset < hidden_parameter_count) {
            return nullptr;
        }

        for (std::size_t i = 0; i < source_parameters.size(); ++i) {
            auto* parameter = source_parameters[i];
            const auto* parameter_type =
                parameter->type != nullptr ? parameter->type->type : nullptr;
            const auto parameter_index = i + offset;
            const auto* lowered_type =
                function_type != nullptr && parameter_index < function_type->parameters.size()
                    ? function_type->parameters[parameter_index].type
                    : nullptr;

            if (auto* flag = cleanup.register_parameter(parameter->name, parameter->span,
                                                        parameter_type, lowered_type);
                flag != nullptr) {
                parameter_flags.push_back(flag);
            }
        }

        auto* lowered_body = static_cast<HIRBlockStatement*>(lower_statement(body));
        lowered_body->statements.insert(lowered_body->statements.begin(), parameter_flags.begin(),
                                        parameter_flags.end());

        if (!terminates(lowered_body)) {
            auto parameter_cleanup = cleanup.emit_scope();
            lowered_body->statements.insert(lowered_body->statements.end(),
                                            parameter_cleanup.begin(), parameter_cleanup.end());
        }

        return lowered_body;
    }

    HIRExpression* resolve_generic_callee(CallExpression& node, const std::string& base_name,
                                          const FunctionType* function_type,
                                          const FunctionDeclaration* definition,
                                          const std::vector<GenericBinding>& argument_types) {
        for (const auto& argument : argument_types) {
            if (!is_concrete_binding(argument)) {
                throw LoweringFailure(node.span,
                                      "generic specialization requires concrete bindings");
            }
        }

        const auto identity = definition != nullptr
                                  ? std::variant<const Node*, const Type*>(definition)
                                  : std::variant<const Node*, const Type*>(function_type);
        const auto instance = mono_cache.get_or_create(identity, base_name, argument_types);

        if (!instance.is_generated && definition != nullptr) {
            auto* specialization =
                lower_monomorphized_function(*definition, instance.name, argument_types);

            if (specialization != nullptr) {
                mono_cache.enqueue_specialization(specialization);
            }
        }

        const Type* lowered_callee_type = nullptr;
        if (definition != nullptr) {
            auto scope = resolver.create_substitution_scope();
            const auto* declared_function_type =
                definition->type != nullptr ? definition->type->as<FunctionType>() : nullptr;
            if (declared_function_type != nullptr) {
                const auto count = std::min(declared_function_type->generic_parameters.size(),
                                            argument_types.size());

                for (std::size_t index = 0; index < count; ++index) {
                    bind_parameter(declared_function_type->generic_parameters[index],
                                   argument_types[index]);
                }
            }
            lowered_callee_type = lower_type(definition->type, node.span);
        } else {
            lowered_callee_type = lower_type(function_type, node.span);
        }

        return program->context.nodes.create<HIRIdentifierExpression>(
            node.callee->span, instance.name, lowered_callee_type);
    }

    HIRCallExpression* build_targeted_call_expression(CallExpression& node,
                                                      std::unique_ptr<HIRCallTarget> target,
                                                      std::vector<HIRExpression*> arguments) {
        auto* call = program->context.nodes.create<HIRCallExpression>(
            node.span, std::move(target), std::move(arguments), lower_type(node.type, node.span));

        return call;
    }

    HIRCallExpression* build_call_expression(CallExpression& node, HIRExpression* callee,
                                             std::vector<HIRExpression*> arguments) {
        if (node.resolved_target.get() != nullptr) {
            if (const auto* direct = node.resolved_target->as<DirectCallTarget>();
                direct != nullptr) {
                const auto* identifier =
                    callee != nullptr ? callee->as<HIRIdentifierExpression>() : nullptr;
                const auto* function_symbol =
                    identifier != nullptr && identifier->function_symbol != nullptr
                        ? identifier->function_symbol
                        : direct->function_symbol;
                return build_targeted_call_expression(
                    node,
                    std::make_unique<HIRDirectCallTarget>(
                        *function_symbol, identifier != nullptr ? identifier->name : std::string(),
                        identifier != nullptr && identifier->type != nullptr
                            ? identifier->type->as<FunctionType>()
                            : nullptr),
                    std::move(arguments));
            } else if (const auto* indirect = node.resolved_target->as<IndirectCallTarget>();
                       indirect != nullptr) {
                if (callee == nullptr) {
                    return nullptr;
                }
                return build_targeted_call_expression(
                    node, std::make_unique<HIRIndirectCallTarget>(callee, *indirect->function_type),
                    std::move(arguments));
            } else if (const auto* interface_target =
                           node.resolved_target->as<InterfaceCallTarget>();
                       interface_target != nullptr) {
                return build_targeted_call_expression(
                    node,
                    std::make_unique<HIRInterfaceCallTarget>(*interface_target->method_symbol,
                                                             interface_target->slot),
                    std::move(arguments));
            } else {
                return build_targeted_call_expression(
                    node,
                    std::make_unique<HIRIntrinsicCallTarget>(
                        node.resolved_target->as<IntrinsicCallTarget>()->intrinsic),
                    std::move(arguments));
            }
        }

        if (callee == nullptr || callee->type == nullptr) {
            return nullptr;
        }

        const auto* function_type = callee->type->as<FunctionType>();
        if (function_type == nullptr) {
            return nullptr;
        }

        return build_targeted_call_expression(
            node, std::make_unique<HIRIndirectCallTarget>(callee, *function_type),
            std::move(arguments));
    }

    const FunctionSymbol* call_symbol(const CallExpression& node) {
        if (node.resolved_target.get() != nullptr) {
            const auto* target = node.resolved_target->as<DirectCallTarget>();
            if (target != nullptr) {
                return target->function_symbol;
            }
        }

        if (const auto* identifier = node.callee->as<IdentifierExpression>();
            identifier != nullptr) {
            if (identifier->function_symbol != nullptr) {
                return identifier->function_symbol;
            }
            return sema.env.root_scope->lookup_function(identifier->name);
        }

        return nullptr;
    }

    HIRNode* lower_identifier(IdentifierExpression& node) {
        const auto* var_symbol = node.var_symbol != nullptr
                                     ? node.var_symbol
                                     : sema.env.current_scope->lookup_var(node.name);

        if (auto iterator = captured_variables.find(var_symbol);
            iterator != captured_variables.end()) {
            const auto& [name, pointer_type] = iterator->second;
            auto* pointer = program->context.nodes.create<HIRIdentifierExpression>(node.span, name,
                                                                                   pointer_type);

            return program->context.nodes.create<HIRUnaryExpression>(
                node.span, UnaryOperator::Type::Dereference, pointer, pointer_type->element);
        }

        const auto* source_type =
            var_symbol != nullptr && var_symbol->type != nullptr ? var_symbol->type : node.type;
        const auto* type = lower_type(source_type, node.span);
        auto name = node.name;

        if (var_symbol != nullptr) {
            name = Mangler::linker_name(node.name, var_symbol);
        } else if (type != nullptr) {
            const auto* function_type = type->as<FunctionType>();

            if (function_type != nullptr) {
                const auto& overloads =
                    sema.env.current_scope->lookup_function_overloads(node.name);
                const auto* symbol = node.function_symbol != nullptr
                                         ? node.function_symbol
                                         : sema.env.current_scope->lookup_function(name);

                auto base_name = symbol != nullptr ? symbol->base_name() : node.name;
                name = Mangler::identifier_name(base_name, function_type, symbol, overloads);

                register_function_symbol(
                    name, symbol != nullptr ? symbol->visibility : Visibility::Type::Public,
                    Linkage::Type::External, function_type, node.span, symbol,
                    symbol != nullptr ? symbol->abi : Abi::Type::Language);
            }
        }

        return program->context.nodes.create<HIRIdentifierExpression>(
            node.span, name, type, var_symbol, node.function_symbol);
    }

    HIRNode* lower_call(CallExpression& node) {
        if (auto* closure = node.callee->as<ClosureExpression>();
            closure != nullptr && !closure->captures.empty()) {
            const auto* closure_type =
                closure->type != nullptr ? closure->type->as<FunctionType>() : nullptr;
            if (closure_type == nullptr ||
                closure_type->parameters.size() != node.arguments.size()) {
                return nullptr;
            }

            auto name = cleanup.next_closure_name();
            std::vector<ParameterType> parameters;
            parameters.reserve(closure->captures.size() + closure_type->parameters.size());

            std::vector<HIRExpression*> arguments;
            arguments.reserve(closure->captures.size() + node.arguments.size());

            std::unordered_map<const VariableSymbol*, std::pair<std::string, const PointerType*>>
                closure_captures;

            for (std::size_t index = 0; index < closure->captures.size(); ++index) {
                const auto* symbol = sema.env.current_scope->lookup_var(closure->captures[index]);
                if (symbol == nullptr || symbol->type == nullptr) {
                    return nullptr;
                }

                const auto* value_type = lower_type(symbol->type, node.span);
                const auto* pointer_type = sema.types.create<PointerType>(value_type, true);
                auto parameter_name = "__capture_" + std::to_string(index);
                parameters.emplace_back(parameter_name, pointer_type);
                closure_captures.emplace(symbol, std::make_pair(parameter_name, pointer_type));

                auto* value = program->context.nodes.create<HIRIdentifierExpression>(
                    node.span, Mangler::linker_name(symbol->name, symbol), value_type, symbol);
                arguments.push_back(program->context.nodes.create<HIRUnaryExpression>(
                    node.span, UnaryOperator::Type::AddressOfMut, value, pointer_type));
            }

            for (const auto& parameter : closure_type->parameters) {
                parameters.emplace_back(parameter.name, lower_type(parameter.type, node.span));
            }

            const auto* function_type = sema.types.create<FunctionType>(
                name, lower_type(closure_type->return_type, node.span), std::move(parameters),
                std::vector<GenericParameterType>(), false);
            auto* function_symbol = sema.env.symbols.create<FunctionSymbol>(
                name, node.span, Visibility::Type::Private, Linkage::Type::Internal, function_type);

            HIRBlockStatement* body = nullptr;
            {
                ScopeGuard scope(sema.env.current_scope, sema.env.scopes,
                                 Scope::Kind::Type::Function, "captured_closure");

                for (std::size_t index = 0; index < closure->parameters.size(); ++index) {
                    const auto* parameter_type =
                        function_type->parameters[index + closure->captures.size()].type;
                    sema.env.current_scope->define_var(
                        closure->parameters[index]->name,
                        sema.env.symbols.create<VariableSymbol>(
                            closure->parameters[index]->name, closure->parameters[index]->span,
                            Visibility::Type::Private, StorageKind::Type::Var, parameter_type));
                }

                auto saved_captures = std::move(captured_variables);
                captured_variables = std::move(closure_captures);
                body = build_owned_body(closure->parameters, function_type, *closure->body,
                                        closure->captures.size());
                captured_variables = std::move(saved_captures);
            }

            if (body == nullptr) {
                return nullptr;
            }

            program->statements.push_back(program->context.nodes.create<HIRFunctionDeclaration>(
                node.span, Visibility::Type::Private, Linkage::Type::Internal, name,
                function_type->return_type, body, false, function_type, false, Abi::Type::Language,
                function_symbol));

            for (const auto* argument : node.arguments) {
                auto* value = lower_expression(*argument->value);
                if (value == nullptr) {
                    return nullptr;
                }
                arguments.push_back(value);
            }

            return build_targeted_call_expression(
                node, std::make_unique<HIRDirectCallTarget>(*function_symbol, name, function_type),
                std::move(arguments));
        }

        const auto* function_symbol = call_symbol(node);
        if (node.resolved_target.get() == nullptr && function_symbol != nullptr) {
            node.resolved_target = std::make_unique<DirectCallTarget>(*function_symbol);
        }
        std::vector<HIRExpression*> arguments;
        arguments.reserve(node.arguments.size());

        for (const auto* argument : node.arguments) {
            arguments.push_back(lower_expression(*argument->value));
        }

        if (function_symbol != nullptr) {
            const auto* function_type = function_symbol->function_type;
            if (function_type != nullptr && function_type->generic_parameters.empty()) {
                const auto* resolved = resolver.resolve_type(function_type);
                function_type = resolved != nullptr ? resolved->as<FunctionType>() : nullptr;
            }

            if (function_type == nullptr) {
                return nullptr;
            }

            auto base_name = function_symbol->base_name();
            auto specialization_name =
                Mangler::function_name(base_name, function_symbol, function_type);

            if (function_symbol->callable_kind == FunctionSymbol::Kind::Type::Constructor) {
                const auto* nominal = node.type != nullptr ? node.type->as_nominal() : nullptr;
                if (nominal != nullptr && !nominal->generic_arguments.empty()) {
                    const auto argument_types =
                        lower_generic_arguments(nominal->generic_arguments, node.span);
                    const auto* definition = mono_cache.get_function(function_symbol);

                    auto* callee = resolve_generic_callee(node, specialization_name, function_type,
                                                          definition, argument_types);

                    return build_call_expression(node, callee, std::move(arguments));
                }
            }

            if (!node.generic_arguments.empty()) {
                const auto argument_types =
                    lower_generic_arguments(node.generic_arguments, node.span);
                const auto* definition = mono_cache.get_function(function_symbol);

                auto* callee = resolve_generic_callee(node, specialization_name, function_type,
                                                      definition, argument_types);

                return build_call_expression(node, callee, std::move(arguments));
            }

            if (!node.arguments.empty()) {
                auto* receiver_type = node.arguments.front()->value->type;
                if (const auto* pointer_type =
                        receiver_type != nullptr ? receiver_type->as<PointerType>() : nullptr;
                    pointer_type != nullptr) {
                    receiver_type = pointer_type->element;
                }
                const auto* receiver_nominal =
                    receiver_type != nullptr ? receiver_type->as_nominal() : nullptr;
                if (receiver_nominal != nullptr && !receiver_nominal->generic_arguments.empty() &&
                    function_symbol->function_type != nullptr &&
                    !function_symbol->function_type->generic_parameters.empty()) {
                    const auto argument_types =
                        lower_generic_arguments(receiver_nominal->generic_arguments, node.span);
                    const auto* definition = mono_cache.get_function(function_symbol);

                    auto* callee = resolve_generic_callee(node, specialization_name, function_type,
                                                          definition, argument_types);

                    return build_call_expression(node, callee, std::move(arguments));
                }
            }

            const auto* lowered_function_type = [&]() -> const FunctionType* {
                auto scope = resolver.create_substitution_scope();

                if (const auto* member = node.callee->as<MemberExpression>();
                    member != nullptr && member->value->type != nullptr) {
                    apply_parent_substitutions(*function_symbol, member->value->type);
                }

                return lower_type(function_type, node.span)->as<FunctionType>();
            }();
            auto name = Mangler::function_name(base_name, function_symbol, lowered_function_type);

            register_function_symbol(
                name,
                function_symbol != nullptr ? function_symbol->visibility : Visibility::Type::Public,
                Linkage::Type::External, lowered_function_type, node.span, function_symbol,
                function_symbol != nullptr ? function_symbol->abi : Abi::Type::Language);

            auto* callee = program->context.nodes.create<HIRIdentifierExpression>(
                node.callee->span, name, lowered_function_type);

            return build_call_expression(node, callee, std::move(arguments));
        }

        const auto* callee_type = node.callee->type;
        const auto* function_type =
            callee_type != nullptr ? callee_type->as<FunctionType>() : nullptr;

        if (function_type == nullptr || node.generic_arguments.empty()) {
            auto* callee = lower_expression(*node.callee);

            return build_call_expression(node, callee, std::move(arguments));
        }

        const auto argument_types = lower_generic_arguments(node.generic_arguments, node.span);
        const auto* definition = mono_cache.get_function(function_type->name);

        auto* callee = resolve_generic_callee(node, function_type->name, function_type, definition,
                                              argument_types);

        return build_call_expression(node, callee, std::move(arguments));
    }

    void register_type_symbol(const std::string& name, Visibility::Type visibility,
                              const Type* type, Span span) {
        if (sema.env.root_scope->lookup_type(name) != nullptr) {
            return;
        }

        auto* symbol = sema.env.symbols.create<TypeSymbol>(name, span, visibility, type);
        sema.env.root_scope->define_type(name, symbol);
    }

    void register_function_symbol(const std::string& name, Visibility::Type visibility,
                                  Linkage::Type linkage, const Type* type, Span span,
                                  const FunctionSymbol* source_symbol = nullptr,
                                  Abi::Type abi = Abi::Type::Language) {
        if (sema.env.root_scope->lookup_function(name) != nullptr) {
            return;
        }

        auto callable_kind = source_symbol != nullptr ? source_symbol->callable_kind
                                                      : FunctionSymbol::Kind::Type::Function;
        auto parent = source_symbol != nullptr ? source_symbol->parent : std::string();

        auto attributes =
            source_symbol != nullptr ? source_symbol->attributes : std::vector<AttributeInfo>();
        auto is_extension = source_symbol != nullptr && source_symbol->is_extension;

        auto* symbol = sema.env.symbols.create<FunctionSymbol>(
            name, span, visibility, linkage, type, callable_kind, parent, std::move(attributes),
            is_extension, abi);
        sema.env.root_scope->define_function(name, symbol,
                                             sema.env.overloads.create<OverloadSet>());
    }

    void register_var_symbol(const std::string& name, Visibility::Type visibility,
                             StorageKind::Type storage_kind, const Type* type, Span span,
                             std::vector<AttributeInfo> attributes = {}) {
        if (sema.env.root_scope->lookup_var(name) != nullptr) {
            return;
        }

        auto* symbol = sema.env.symbols.create<VariableSymbol>(name, span, visibility, storage_kind,
                                                               type, std::move(attributes));
        sema.env.root_scope->define_var(name, symbol);
    }

    void register_struct(StructDeclaration& node) {
        if (!node.generic_parameters.empty()) {
            mono_cache.register_struct(node.name, &node);
            return;
        }

        const auto* lowered_type = lower_type(node.type, node.span);
        if (!resolver.is_concrete(lowered_type)) {
            return;
        }
        const auto* symbol = sema.env.current_scope->lookup_type(node.name);

        register_type_symbol(Mangler::linker_name(node.name, symbol), node.visibility, lowered_type,
                             node.span);
    }

    void register_interface(InterfaceDeclaration& node) {
        if (!node.generic_parameters.empty()) {
            return;
        }

        const auto* lowered_type = lower_type(node.type, node.span);
        if (!resolver.is_concrete(lowered_type)) {
            return;
        }
        const auto* symbol = sema.env.current_scope->lookup_type(node.name);

        register_type_symbol(Mangler::linker_name(node.name, symbol), node.visibility, lowered_type,
                             node.span);
    }

    void register_enum(EnumDeclaration& node) {
        if (!node.generic_parameters.empty()) {
            return;
        }

        const auto* lowered_type = lower_type(node.type, node.span);
        if (!resolver.is_concrete(lowered_type)) {
            return;
        }
        const auto* symbol = sema.env.current_scope->lookup_type(node.name);

        register_type_symbol(Mangler::linker_name(node.name, symbol), node.visibility, lowered_type,
                             node.span);
    }

    void register_function(FunctionDeclaration& node) {
        if (!node.prototype->generic_parameters.empty()) {
            auto key = node.function_symbol != nullptr ? node.function_symbol->base_name()
                                                       : node.prototype->name;
            mono_cache.register_function(key, &node, node.function_symbol);
            return;
        }

        const auto* lowered_type = lower_type(node.type, node.span);
        const auto* function_type =
            lowered_type != nullptr ? lowered_type->as<FunctionType>() : nullptr;
        if (function_type == nullptr) {
            return;
        }

        if (!is_emittable_function_type(function_type)) {
            auto key = node.function_symbol != nullptr ? node.function_symbol->base_name()
                                                       : node.prototype->name;
            mono_cache.register_function(key, &node, node.function_symbol);
            return;
        }

        const auto* symbol = node.function_symbol;
        auto base_name = symbol != nullptr ? symbol->base_name() : node.prototype->name;
        auto name = Mangler::function_name(base_name, symbol, function_type);
        auto linkage = Mangler::function_linkage(base_name, symbol);

        register_function_symbol(name, node.visibility, linkage, function_type, node.span, symbol);
    }

    void register_extern_function(ExternFunctionDeclaration& node) {
        const auto* lowered_type = lower_type(node.type, node.span);
        const auto* function_type =
            lowered_type != nullptr ? lowered_type->as<FunctionType>() : nullptr;
        if (function_type == nullptr) {
            return;
        }
        if (!is_emittable_function_type(function_type)) {
            return;
        }

        const auto* symbol = node.function_symbol;
        auto name = Mangler::function_name(node.prototype->name, symbol, function_type);

        register_function_symbol(name, node.visibility, Linkage::Type::External, function_type,
                                 node.span, symbol, Abi::Type::C);
    }

    void register_variable(VarDeclaration& node) {
        const auto* lowered_type = lower_type(node.type, node.span);
        const auto* symbol = node.variable_symbol;
        auto attributes = symbol != nullptr ? symbol->attributes : std::vector<AttributeInfo>();

        register_var_symbol(Mangler::linker_name(node.name, symbol), node.visibility,
                            node.storage_kind, lowered_type, node.span, std::move(attributes));
    }

    void register_extern_variable(ExternVarDeclaration& node) {
        const auto* lowered_type = lower_type(node.type, node.span);
        const auto* symbol = node.variable_symbol;
        auto attributes = symbol != nullptr ? symbol->attributes : std::vector<AttributeInfo>();

        register_var_symbol(Mangler::linker_name(node.name, symbol), node.visibility,
                            StorageKind::Type::Var, lowered_type, node.span, std::move(attributes));
    }

    void register_program_symbols(Program& ast_program) {
        for (auto& statement : ast_program.statements) {
            if (auto* struct_declaration = statement->as<StructDeclaration>();
                struct_declaration != nullptr) {
                register_struct(*struct_declaration);

                for (auto* method : struct_declaration->methods) {
                    register_function(*method);
                }
            }

            if (auto* interface_declaration = statement->as<InterfaceDeclaration>();
                interface_declaration != nullptr) {
                register_interface(*interface_declaration);
            }

            if (auto* enum_declaration = statement->as<EnumDeclaration>();
                enum_declaration != nullptr) {
                register_enum(*enum_declaration);

                for (auto* method : enum_declaration->methods) {
                    register_function(*method);
                }
            }

            if (auto* function_declaration = statement->as<FunctionDeclaration>();
                function_declaration != nullptr) {
                register_function(*function_declaration);
            }

            if (auto* extern_function_declaration = statement->as<ExternFunctionDeclaration>();
                extern_function_declaration != nullptr) {
                register_extern_function(*extern_function_declaration);
            }

            if (auto* variable_declaration = statement->as<VarDeclaration>();
                variable_declaration != nullptr) {
                register_variable(*variable_declaration);
            }

            if (auto* extern_variable_declaration = statement->as<ExternVarDeclaration>();
                extern_variable_declaration != nullptr) {
                register_extern_variable(*extern_variable_declaration);
            }
        }
    }

    void drain_specializations() {
        while (true) {
            std::vector<HIRFunctionDeclaration*> pending;
            mono_cache.drain_pending_specializations_into(pending);

            if (pending.empty()) {
                break;
            }

            for (auto* function : pending) {
                register_function_symbol(function->name, function->visibility, function->linkage,
                                         function->type, function->span);
                program->statements.push_back(function);
            }
        }
    }

  public:
    using Visitor<HIRNode*>::visit;
    Diagnostics& diagnostics;

    explicit HIRLowerer(Context& context, SemaContext& sema)
        : sema(sema), context(context), program(std::make_shared<HIRProgram>()),
          resolver(sema.types, sema.env, context.diagnostics), facades(sema, resolver),
          cleanup(*program, sema, resolver), diagnostics(context.diagnostics) {}

    HIRNode* visit_expression(Expression& node) {
        return Visitor<HIRNode*>::visit_expression(node);
    }

    HIRNode* visit_statement(Statement& node) { return Visitor<HIRNode*>::visit_statement(node); }

    MonomorphizationCache& monomorphizations() { return mono_cache; }

    bool is_lowering_monomorphized_body() const { return lowering_monomorphized_body; }

    HIRNode* visit(TypeExpression& node) override {
        const auto* lowered = lower_type(node.type, node.span);

        return program->context.nodes.create<HIRTypeExpression>(node.span, lowered);
    }

    HIRNode* visit(NumberLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->context.nodes.create<HIRNumberLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(FloatLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->context.nodes.create<HIRFloatLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(StringLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->context.nodes.create<HIRStringLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(CharLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->context.nodes.create<HIRCharLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(BooleanLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->context.nodes.create<HIRBooleanLiteral>(node.span, node.value, type);
    }

    HIRNode* visit(NullLiteral& node) override {
        const auto* type = lower_type(node.type, node.span);
        return program->context.nodes.create<HIRNullLiteral>(node.span, type);
    }

    HIRNode* visit(IdentifierExpression& node) override {
        if (node.compile_time_value.has_value()) {
            if (auto* lowered = lower_compile_time_value(node.span, *node.compile_time_value);
                lowered != nullptr) {
                return lowered;
            }
        }

        if (node.var_symbol == nullptr) {
            const auto binding = resolver.const_parameter(node.name);
            if (binding.has_value()) {
                if (!binding->value.has_value()) {
                    throw LoweringFailure(
                        node.span, "runtime expression requires a concrete const generic argument");
                }
                return lower_compile_time_value(node.span, *binding->value);
            }
        }

        if (node.implicit_self_field) {
            const auto* self_symbol = sema.env.current_scope->lookup_var("self");
            const auto* self_type =
                self_symbol != nullptr ? lower_type(self_symbol->type, node.span) : nullptr;
            const auto* pointer_type =
                self_type != nullptr ? self_type->as<PointerType>() : nullptr;
            const auto* object_type = pointer_type != nullptr ? pointer_type->element : nullptr;

            auto* self = program->context.nodes.create<HIRIdentifierExpression>(node.span, "self",
                                                                                self_type);
            auto* object = program->context.nodes.create<HIRUnaryExpression>(
                node.span, UnaryOperator::Type::Dereference, self, object_type);

            if (node.name == "value" && object_type != nullptr && !object_type->is<StructType>()) {
                return object;
            }

            return program->context.nodes.create<HIRMemberExpression>(
                node.span, object, node.name, lower_type(node.type, node.span));
        }

        return lower_identifier(node);
    }

    HIRNode* visit(BinaryExpression& node) override {
        auto* left = lower_expression(*node.left);
        auto* right = lower_expression(*node.right);
        const auto* type = node.op == BinaryExpression::Operator::Type::As && right != nullptr &&
                                   right->as<HIRTypeExpression>() != nullptr
                               ? right->as<HIRTypeExpression>()->type_value
                               : lower_type(node.type, node.span);

        return program->context.nodes.create<HIRBinaryExpression>(
            node.span, left, static_cast<BinaryOperator::Type>(node.op), right, type);
    }

    HIRNode* visit(UnaryExpression& node) override {
        auto* operand = lower_expression(*node.operand);
        if (operand == nullptr) {
            return nullptr;
        }

        const auto* pointer_type =
            operand->type != nullptr ? operand->type->as<PointerType>() : nullptr;
        const auto* type =
            node.op == UnaryExpression::Operator::Type::Dereference && pointer_type != nullptr
                ? pointer_type->element
                : lower_type(node.type, node.span);

        return program->context.nodes.create<HIRUnaryExpression>(
            node.span, static_cast<UnaryOperator::Type>(node.op), operand, type);
    }

    HIRNode* visit(CoerceExpression& node) override {
        auto* value = lower_expression(*node.value);
        const auto* type = lower_type(node.type, node.span);
        const auto* source_type = lower_type(node.source_type, node.span);

        return program->context.nodes.create<HIRCoerceExpression>(node.span, value, type,
                                                                  node.coercion, source_type);
    }

    HIRNode* visit(BuiltinCall& node) override {
        if (node.name == "sizeof") {
            if (node.type_argument == nullptr || node.type_argument->type == nullptr) {
                return nullptr;
            }

            const auto* sized_type = lower_type(node.type_argument->source_type, node.span);
            auto size = compile_time_size_of(sized_type);
            if (!size.has_value() && node.type_argument->type != node.type_argument->source_type) {
                sized_type = lower_type(node.type_argument->type, node.span);
                size = compile_time_size_of(sized_type);
            }

            if (!size.has_value()) {
                throw LoweringFailure(node.span, "sizeof requires a concrete sized type");
            }
            const auto* type = lower_type(node.type, node.span);

            return program->context.nodes.create<HIRNumberLiteral>(node.span, std::to_string(*size),
                                                                   type);
        }

        if (node.name == "length") {
            if (node.arguments.size() != 1 || node.arguments[0]->type == nullptr) {
                return nullptr;
            }

            const auto* argument_type = node.arguments[0]->type;
            if (const auto* identifier = node.arguments[0]->as<IdentifierExpression>();
                identifier != nullptr) {
                const auto* symbol = sema.env.current_scope->lookup_var(identifier->name);
                if (symbol != nullptr) {
                    argument_type = symbol->type;
                }
            }

            const auto* lowered_argument_type = lower_type(argument_type, node.span);
            const auto* array_type =
                lowered_argument_type != nullptr ? lowered_argument_type->as<ArrayType>() : nullptr;
            if (array_type == nullptr) {
                return nullptr;
            }

            auto size = std::optional<std::size_t>();
            if (const auto* concrete = std::get_if<ConcreteArrayExtent>(&array_type->extent);
                concrete != nullptr) {
                size = concrete->value;
            } else if (const auto* dependent =
                           std::get_if<DependentArrayExtent>(&array_type->extent);
                       dependent != nullptr && dependent->expression != nullptr) {
                Evaluator evaluator(diagnostics, &sema.env, &resolver.environment());
                auto* expression =
                    const_cast<Expression*>(static_cast<const Expression*>(dependent->expression));
                const auto value = evaluator.evaluate_uncached(*expression);
                const auto resolved_size =
                    value.has_value() ? value->try_as_unsigned_integer() : std::nullopt;
                if (resolved_size.has_value()) {
                    size = static_cast<std::size_t>(*resolved_size);
                }
            }

            if (!size.has_value()) {
                return nullptr;
            }

            const auto* type = lower_type(node.type, node.span);

            return program->context.nodes.create<HIRNumberLiteral>(node.span, std::to_string(*size),
                                                                   type);
        }

        if (node.name == "asm") {
            if (node.arguments.size() != 1) {
                return nullptr;
            }

            auto* lowered_arg = lower_expression(*node.arguments[0]);
            if (lowered_arg == nullptr) {
                return nullptr;
            }

            const auto* type = lower_type(node.type, node.span);
            std::vector<HIRExpression*> args = {lowered_arg};

            return program->context.nodes.create<HIRCallExpression>(
                node.span,
                std::make_unique<HIRIntrinsicCallTarget>(Intrinsic::Type::InlineAssembly),
                std::move(args), type);
        }

        return nullptr;
    }

    HIRNode* visit(CallExpression& node) override {
        if (node.compile_time_value.has_value()) {
            return lower_compile_time_value(node.span, *node.compile_time_value);
        }

        auto* result = lower_call(node);

        const auto* function_symbol = call_symbol(node);
        if (function_symbol == nullptr ||
            function_symbol->callable_kind != FunctionSymbol::Kind::Type::Destructor) {
            return result;
        }

        return cleanup.clear_after_destructor_call(node, static_cast<HIRExpression*>(result));
    }

    HIRNode* visit(IndexExpression& node) override {
        auto* value = lower_expression(*node.value);
        auto* index = lower_expression(*node.index);
        const Type* type = nullptr;

        if (const auto* array_type =
                value != nullptr && value->type != nullptr ? value->type->as<ArrayType>() : nullptr;
            array_type != nullptr) {
            type = array_type->element;
        } else if (const auto* pointer_type = value != nullptr && value->type != nullptr
                                                  ? value->type->as<PointerType>()
                                                  : nullptr;
                   pointer_type != nullptr) {
            type = pointer_type->element;
        } else if (value != nullptr && value->type != nullptr && value->type->is<StringType>()) {
            type = sema.builtin_resolver.primitives.at("char");
        } else {
            type = lower_type(node.type, node.span);
        }

        return program->context.nodes.create<HIRIndexExpression>(node.span, value, index, type);
    }

    HIRNode* visit(MemberExpression& node) override {
        if (node.compile_time_value.has_value()) {
            return lower_compile_time_value(node.span, *node.compile_time_value);
        }
        auto* value = lower_expression(*node.value);
        const Type* type = nullptr;

        const auto* lowered_value_type = value != nullptr ? value->type : nullptr;
        if (const auto* pointer_type =
                lowered_value_type != nullptr ? lowered_value_type->as<PointerType>() : nullptr;
            pointer_type != nullptr) {
            lowered_value_type = pointer_type->element;
        }

        if (const auto* struct_type =
                lowered_value_type != nullptr ? lowered_value_type->as<StructType>() : nullptr;
            struct_type != nullptr) {
            if (const auto* field = struct_type->find_field(node.member); field != nullptr) {
                type = field->type;
            }
        }

        if (type == nullptr) {
            type = lower_type(node.type, node.span);
        }

        const auto* enum_type =
            node.value->type != nullptr ? node.value->type->as<EnumType>() : nullptr;
        if (enum_type != nullptr && enum_type->backing_type != nullptr && node.member == "value") {
            return program->context.nodes.create<HIRCoerceExpression>(
                node.span, value, type, Coercion::Type::None,
                lower_type(node.value->type, node.span));
        }

        if (node.member == "value" && facades.resolve_value(node.value->type) != nullptr) {
            return value;
        }

        return program->context.nodes.create<HIRMemberExpression>(node.span, value, node.member,
                                                                  type);
    }

    HIRNode* visit(QualifiedAccessExpression& node) override {
        if (node.variant_type != nullptr) {
            const auto* type = lower_type(node.type, node.span);
            const auto* enum_type = type != nullptr ? type->as<EnumType>() : nullptr;

            if (enum_type == nullptr) {
                return nullptr;
            }

            if (enum_type->backing_type != nullptr) {
                if (!node.enum_value.has_value()) {
                    return nullptr;
                }

                auto* value = lower_compile_time_value(node.span, *node.enum_value);
                return program->context.nodes.create<HIRCoerceExpression>(
                    node.span, value, type, Coercion::Type::None,
                    lower_type(node.enum_value->type, node.span));
            }

            return program->context.nodes.create<HIREnumVariantExpression>(
                node.span, enum_type->name, node.member, std::vector<HIRStructLiteralField>{},
                node.variant_type->index, type);
        }

        if (node.function_symbol == nullptr) {
            return nullptr;
        }

        const auto* type = lower_type(node.type, node.span);
        auto name = Mangler::function_name(node.function_symbol->base_name(), node.function_symbol,
                                           type != nullptr ? type->as<FunctionType>() : nullptr);

        return program->context.nodes.create<HIRIdentifierExpression>(node.span, name, type);
    }

    HIRNode* visit(AssignExpression& node) override {
        auto* target = lower_expression(*node.target);
        auto* value = lower_expression(*node.value);
        const auto* type = lower_type(node.type, node.span);

        if (target != nullptr && type != nullptr && cleanup.needs_destruction(type)) {
            if (auto* identifier = node.target->as<IdentifierExpression>(); identifier != nullptr) {
                auto flag_name = cleanup.drop_flag_for(identifier->name);
                if (!flag_name.empty()) {
                    const auto* boolean_type = sema.builtin_resolver.primitives.at("boolean");
                    const auto* void_type = sema.builtin_resolver.primitives.at("void");

                    std::vector<HIRStatement*> drop_statements;
                    cleanup.build_drop_statements_for_expression(node.span, target, type,
                                                                 drop_statements);

                    if (!drop_statements.empty()) {
                        HIRStatement* drop_body =
                            drop_statements.size() == 1
                                ? drop_statements.front()
                                : program->context.nodes.create<HIRBlockStatement>(
                                      node.span, std::move(drop_statements));

                        auto* condition = program->context.nodes.create<HIRIdentifierExpression>(
                            node.span, flag_name, boolean_type);
                        auto* drop_if = program->context.nodes.create<HIRIfExpression>(
                            node.span, condition, drop_body, nullptr, void_type);
                        auto* drop_stmt = program->context.nodes.create<HIRExpressionStatement>(
                            node.span, drop_if);

                        auto* assign_expr = program->context.nodes.create<HIRAssignExpression>(
                            node.span, target, value, type);
                        auto* assign_stmt = program->context.nodes.create<HIRExpressionStatement>(
                            node.span, assign_expr);

                        auto* true_lit = program->context.nodes.create<HIRBooleanLiteral>(
                            node.span, true, boolean_type);
                        auto* flag_id = program->context.nodes.create<HIRIdentifierExpression>(
                            node.span, flag_name, boolean_type);
                        auto* rearm_expr = program->context.nodes.create<HIRAssignExpression>(
                            node.span, flag_id, true_lit, boolean_type);
                        auto* rearm_stmt = program->context.nodes.create<HIRExpressionStatement>(
                            node.span, rearm_expr);

                        std::vector<HIRStatement*> group;
                        group.push_back(drop_stmt);
                        group.push_back(assign_stmt);
                        group.push_back(rearm_stmt);

                        return program->context.nodes.create<HIRStatementGroup>(node.span,
                                                                                std::move(group));
                    }
                }
            }
        }

        return program->context.nodes.create<HIRAssignExpression>(node.span, target, value, type);
    }

    HIRNode* visit(StructLiteralExpression& node) override {
        if (auto* value = lower_facade_literal(node); value != nullptr) {
            return value;
        }

        auto fields = lower_literal_fields(node.fields);

        const auto* type = lower_type(node.type, node.span);
        if (type == nullptr) {
            return nullptr;
        }

        if (const auto* enum_type = type->as<EnumType>(); enum_type != nullptr) {
            auto* qualified = node.name->as<QualifiedAccessExpression>();
            if (qualified == nullptr || qualified->variant_type == nullptr) {
                return nullptr;
            }

            return program->context.nodes.create<HIREnumVariantExpression>(
                node.span, enum_type->name, qualified->member, std::move(fields),
                qualified->variant_type->index, type);
        }

        const auto* struct_type = type->as<StructType>();
        if (struct_type == nullptr) {
            return nullptr;
        }

        return program->context.nodes.create<HIRStructLiteralExpression>(
            node.span, struct_type->name, std::move(fields), type);
    }

    HIRNode* visit(EnumVariantExpression& node) override {
        auto fields = lower_literal_fields(node.fields);

        const auto* type = lower_type(node.type, node.span);
        if (type == nullptr) {
            return nullptr;
        }

        const auto* enum_type = type->as<EnumType>();
        if (enum_type == nullptr || node.variant_type == nullptr) {
            return nullptr;
        }

        return program->context.nodes.create<HIREnumVariantExpression>(
            node.span, enum_type->name, node.variant_name, std::move(fields),
            node.variant_type->index, type);
    }

    HIRNode* visit(IfExpression& node) override {
        auto* condition = lower_expression(*node.condition);
        auto* then_branch = lower_statement(*node.then_branch);
        auto* else_branch =
            node.else_branch != nullptr ? lower_statement(*node.else_branch) : nullptr;
        const auto* type = lower_type(node.type, node.span);

        return program->context.nodes.create<HIRIfExpression>(node.span, condition, then_branch,
                                                              else_branch, type);
    }

    HIRNode* visit(WhenArm& node [[maybe_unused]]) override { return nullptr; }

    HIRNode* visit(WhenExpression& node) override {
        auto* subject = node.subject != nullptr ? lower_expression(*node.subject) : nullptr;

        std::vector<HIRWhenArm> arms;
        arms.reserve(node.arms.size());

        for (auto* arm : node.arms) {
            ScopeGuard arm_scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Block,
                                 "when_arm");

            std::vector<HIRWhenPattern> patterns;
            patterns.reserve(arm->patterns.size());

            for (auto* pattern : arm->patterns) {
                if (pattern->pattern_kind == WhenPattern::PatternKind::Type::Expression) {
                    patterns.emplace_back(lower_expression(*pattern->expression));
                    continue;
                }

                std::vector<HIRWhenPatternField> fields;
                fields.reserve(pattern->fields.size());

                if (pattern->enum_type != nullptr && pattern->variant_type != nullptr) {
                    for (auto* field : pattern->fields) {
                        const auto* declared_field =
                            pattern->variant_type->find_field(field->field_name);
                        const auto* field_type = declared_field != nullptr
                                                     ? lower_type(declared_field->type, field->span)
                                                     : nullptr;

                        sema.env.current_scope->define_var(field->binding_name,
                                                           sema.env.symbols.create<VariableSymbol>(
                                                               field->binding_name, field->span,
                                                               Visibility::Type::Private,
                                                               StorageKind::Type::Var, field_type));

                        fields.emplace_back(field->field_name, field->binding_name, field_type);
                    }
                }

                const auto variant_index =
                    pattern->variant_type != nullptr ? pattern->variant_type->index : 0;
                const auto enum_name =
                    pattern->enum_type != nullptr ? pattern->enum_type->name : std::string();
                HIRExpression* variant_value = nullptr;
                if (pattern->enum_type != nullptr && pattern->enum_type->backing_type != nullptr &&
                    pattern->variant_type != nullptr) {
                    if (pattern->variant_type->discriminant.has_value()) {
                        variant_value = lower_compile_time_value(
                            pattern->span, *pattern->variant_type->discriminant);
                    }
                }

                patterns.emplace_back(enum_name, pattern->variant_name, variant_index,
                                      variant_value, std::move(fields));
            }

            auto* guard = arm->guard != nullptr ? lower_expression(*arm->guard) : nullptr;
            auto* body = lower_statement(*arm->body);

            arms.emplace_back(arm->is_else, std::move(patterns), guard, body);
        }

        const auto* type = lower_type(node.type, node.span);

        return program->context.nodes.create<HIRWhenExpression>(node.span, subject, std::move(arms),
                                                                node.is_exhaustive, type);
    }

    HIRNode* visit(WhileStatement& node) override {
        auto* condition = lower_expression(*node.condition);
        auto* body = lower_statement(*node.body);
        const auto* type = lower_type(node.type, node.span);

        return program->context.nodes.create<HIRLoopStatement>(
            node.span, std::vector<HIRStatement*>{}, condition, nullptr, body, type);
    }

    HIRNode* visit(ForStatement& node) override {
        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Block, "for");

        std::vector<HIRStatement*> initializers;
        initializers.reserve(1);

        if (auto* initializer = lower_statement(*node.initializer); initializer != nullptr) {
            initializers.push_back(initializer);
        }

        auto* condition = lower_expression(*node.condition);
        auto* step = node.step != nullptr ? lower_expression(*node.step) : nullptr;
        auto* body = lower_statement(*node.body);
        const auto* type = lower_type(node.type, node.span);

        return program->context.nodes.create<HIRLoopStatement>(node.span, std::move(initializers),
                                                               condition, step, body, type);
    }

    HIRNode* visit(ClosureExpression& node) override {
        auto name = cleanup.next_closure_name();
        const auto* type_base = lower_type(node.type, node.span);
        const auto* type = type_base != nullptr ? type_base->as<FunctionType>() : nullptr;

        if (type == nullptr) {
            return nullptr;
        }

        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Function,
                         "closure");

        for (std::size_t i = 0; i < node.parameters.size(); ++i) {
            sema.env.current_scope->define_var(
                node.parameters[i]->name,
                sema.env.symbols.create<VariableSymbol>(
                    node.parameters[i]->name, node.span, Visibility::Type::Private,
                    StorageKind::Type::Var, type->parameters[i].type));
        }

        auto* body = build_owned_body(node.parameters, type, *node.body);

        auto* function = program->context.nodes.create<HIRFunctionDeclaration>(
            node.span, Visibility::Type::Private, Linkage::Type::Internal, name, type->return_type,
            body, false, type, false);

        program->statements.push_back(function);

        return program->context.nodes.create<HIRIdentifierExpression>(node.span, name, type);
    }

    HIRNode* visit(BlockExpression& node) override {
        auto* body = lower_block(*node.body);
        return program->context.nodes.create<HIRBlockExpression>(node.span, body,
                                                                 lower_type(node.type, node.span));
    }

    HIRNode* visit(ArrayLiteralExpression& node) override {
        const auto* type = lower_type(node.type, node.span);
        if (type == nullptr) {
            return nullptr;
        }

        std::vector<HIRExpression*> elements;
        elements.reserve(node.elements.size());
        for (auto* element : node.elements) {
            auto* lowered = lower_expression(*element);
            if (lowered == nullptr) {
                return nullptr;
            }

            elements.push_back(lowered);
        }

        return program->context.nodes.create<HIRArrayLiteralExpression>(node.span,
                                                                        std::move(elements), type);
    }

    HIRNode* visit(BlockStatement& node) override {
        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Block,
                         "block");
        CleanupScopeGuard cleanup_guard(cleanup);

        std::vector<HIRStatement*> body;
        body.reserve(node.statements.size());
        auto terminated = false;

        for (auto* statement : node.statements) {
            if (auto* lowered = lower_statement(*statement); lowered != nullptr) {
                body.push_back(lowered);

                if (terminates(lowered)) {
                    terminated = true;
                    break;
                }
            }
        }

        if (!terminated) {
            auto scope_cleanup = cleanup.emit_scope();
            body.insert(body.end(), scope_cleanup.begin(), scope_cleanup.end());
        }

        return program->context.nodes.create<HIRBlockStatement>(node.span, std::move(body));
    }

    HIRNode* visit(ExpressionStatement& node) override {
        auto* expression = lower_expression(*node.expression);

        return program->context.nodes.create<HIRExpressionStatement>(node.span, expression);
    }

    HIRNode* visit(ReturnStatement& node) override {
        auto* value = node.value != nullptr ? lower_expression(*node.value) : nullptr;

        auto cleanup_statements = cleanup.emit_all();
        if (cleanup_statements.empty()) {
            return program->context.nodes.create<HIRReturnStatement>(node.span, value);
        }

        if (value == nullptr) {
            cleanup_statements.push_back(
                program->context.nodes.create<HIRReturnStatement>(node.span, nullptr));

            return program->context.nodes.create<HIRBlockStatement>(node.span,
                                                                    std::move(cleanup_statements));
        }

        auto name = cleanup.next_return_tmp_name();
        auto* temporary = program->context.nodes.create<HIRVarDeclaration>(
            node.span, Visibility::Type::Private, Linkage::Type::Internal, StorageKind::Type::Var,
            name, value->type, value, false);
        auto* temporary_value =
            program->context.nodes.create<HIRIdentifierExpression>(node.span, name, value->type);

        std::vector<HIRStatement*> statements;
        statements.reserve(cleanup_statements.size() + 2);
        statements.push_back(temporary);
        statements.insert(statements.end(), cleanup_statements.begin(), cleanup_statements.end());
        statements.push_back(
            program->context.nodes.create<HIRReturnStatement>(node.span, temporary_value));

        return program->context.nodes.create<HIRBlockStatement>(node.span, std::move(statements));
    }

    HIRNode* visit(DeferStatement& node) override {
        auto* body = lower_statement(*node.body);
        cleanup.register_defer(node.span, body);

        return nullptr;
    }

    HIRNode* visit(VarDeclaration& node) override {
        auto* initializer =
            node.initializer != nullptr ? lower_expression(*node.initializer) : nullptr;
        const auto* source_type = node.type != nullptr     ? node.type
                                  : initializer != nullptr ? initializer->type
                                                           : nullptr;
        const auto* type = lower_type(source_type, node.span);

        const auto* symbol = node.variable_symbol;
        auto name = Mangler::linker_name(node.name, symbol);

        auto* declaration = program->context.nodes.create<HIRVarDeclaration>(
            node.span, node.visibility, Linkage::Type::Internal, node.storage_kind, std::move(name),
            type, initializer, sema.env.current_scope->is_global(),
            AttributeResolver::convert(node.attributes));

        if (!sema.env.current_scope->is_global() &&
            !sema.env.current_scope->has_local_var(node.name)) {
            sema.env.current_scope->define_var(
                node.name, sema.env.symbols.create<VariableSymbol>(
                               node.name, node.span, node.visibility, node.storage_kind, type));
        }

        if (is_lowering_monomorphized_body() && !sema.env.current_scope->has_local_var(node.name)) {
            sema.env.current_scope->define_var(
                node.name, sema.env.symbols.create<VariableSymbol>(
                               node.name, node.span, node.visibility, node.storage_kind, type));
        }

        if (auto* flag = cleanup.register_var(node, declaration->name, type); flag != nullptr) {
            std::vector<HIRStatement*> statements;
            statements.reserve(2);
            statements.push_back(declaration);
            statements.push_back(flag);

            return program->context.nodes.create<HIRStatementGroup>(node.span,
                                                                    std::move(statements));
        }

        return declaration;
    }

    HIRNode* visit(ExternVarDeclaration& node) override {
        const auto* type = lower_type(node.type, node.span);
        const auto* symbol = node.variable_symbol;
        auto name = Mangler::linker_name(node.name, symbol);

        return program->context.nodes.create<HIRVarDeclaration>(
            node.span, node.visibility, Linkage::Type::External, StorageKind::Type::Var,
            std::move(name), type, nullptr, true, AttributeResolver::convert(node.attributes));
    }

    HIRNode* visit(FunctionDeclaration& node) override {
        if (!node.prototype->generic_parameters.empty()) {
            return nullptr;
        }

        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Function,
                         node.prototype->name);

        auto variadic = node.prototype->is_variadic;
        const auto* return_type = lower_type(node.prototype->return_type->type, node.span);
        const auto* type = lower_function_type(node.type, node.span);
        if (type == nullptr || !is_emittable_function_type(type)) {
            return nullptr;
        }

        define_parameters(*node.prototype, type);

        const auto* symbol = node.function_symbol;
        auto base_name = symbol != nullptr ? symbol->base_name() : node.prototype->name;
        auto name = Mangler::function_name(base_name, symbol, type);
        auto linkage = Mangler::function_linkage(base_name, symbol);
        auto mangled = Mangler::is_mangled(symbol);

        const auto* previous_function = current_function;
        auto previous_parent = current_parent;
        current_function = symbol;
        current_parent = node.parent;
        auto* body = build_owned_body(node.prototype->parameters, type, *node.body);
        current_function = previous_function;
        current_parent = std::move(previous_parent);

        return program->context.nodes.create<HIRFunctionDeclaration>(
            node.span, node.visibility, linkage, std::move(name), return_type, body, variadic, type,
            mangled, Abi::Type::Language, symbol);
    }

    HIRNode* visit(ExternFunctionDeclaration& node) override {
        auto variadic = node.prototype->is_variadic;
        const auto* return_type = lower_type(node.prototype->return_type->type, node.span);
        const auto* type = lower_function_type(node.type, node.span);

        const auto* symbol = node.function_symbol;
        auto base_name = symbol != nullptr ? symbol->base_name() : node.prototype->name;
        auto name = Mangler::function_name(base_name, symbol, type);
        auto mangled = Mangler::is_mangled(symbol);

        return program->context.nodes.create<HIRFunctionDeclaration>(
            node.span, node.visibility, Linkage::Type::External, std::move(name), return_type,
            nullptr, variadic, type, mangled, Abi::Type::C, symbol);
    }

    HIRNode* visit(StructDeclaration& node) override {
        for (auto* method : node.methods) {
            if (auto* lowered = lower_statement(*method); lowered != nullptr) {
                program->statements.push_back(lowered);
            }
        }

        for (auto* nested_struct : node.structs) {
            visit(*nested_struct);
        }

        for (auto* nested_enum : node.enums) {
            visit(*nested_enum);
        }

        return nullptr;
    }

    HIRNode* visit(EnumDeclaration& node) override {
        for (auto* method : node.methods) {
            if (auto* lowered = lower_statement(*method); lowered != nullptr) {
                program->statements.push_back(lowered);
            }
        }

        return nullptr;
    }

    HIRNode* visit(Parameter& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(Argument& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(FunctionPrototype& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(Field& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(EnumVariant& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(StructLiteralField& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(WhenPatternField& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(WhenPattern& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(GenericParameter& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(GenericArgument& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(Attribute& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(ImportStatement& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(InterfaceDeclaration& node [[maybe_unused]]) override { return nullptr; }
    HIRNode* visit(TypeAliasDeclaration& node [[maybe_unused]]) override { return nullptr; }

    void register_generic_declarations(const std::vector<const Program*>& programs) {
        for (const auto* program_node : programs) {
            if (program_node == nullptr) {
                continue;
            }

            for (auto* statement : program_node->statements) {
                if (auto* struct_declaration = statement->as<StructDeclaration>();
                    struct_declaration != nullptr) {
                    if (!struct_declaration->generic_parameters.empty()) {
                        mono_cache.register_struct(struct_declaration->name, struct_declaration);
                    }

                    for (auto* method : struct_declaration->methods) {
                        if (!method->prototype->generic_parameters.empty() ||
                            !struct_declaration->generic_parameters.empty()) {
                            auto base_name =
                                method->function_symbol != nullptr
                                    ? method->function_symbol->base_name()
                                    : struct_declaration->name + "::" + method->prototype->name;
                            mono_cache.register_function(base_name, method,
                                                         method->function_symbol);
                        }
                    }
                }

                if (auto* enum_declaration = statement->as<EnumDeclaration>();
                    enum_declaration != nullptr) {
                    for (auto* method : enum_declaration->methods) {
                        if (!method->prototype->generic_parameters.empty() ||
                            !enum_declaration->generic_parameters.empty()) {
                            auto base_name =
                                method->function_symbol != nullptr
                                    ? method->function_symbol->base_name()
                                    : enum_declaration->name + "::" + method->prototype->name;
                            mono_cache.register_function(base_name, method,
                                                         method->function_symbol);
                        }
                    }
                }

                if (auto* function_declaration = statement->as<FunctionDeclaration>();
                    function_declaration != nullptr) {
                    if (!function_declaration->prototype->generic_parameters.empty()) {
                        auto base_name = function_declaration->function_symbol != nullptr
                                             ? function_declaration->function_symbol->base_name()
                                             : function_declaration->prototype->name;
                        mono_cache.register_function(base_name, function_declaration,
                                                     function_declaration->function_symbol);
                    }
                }
            }
        }
    }

    std::shared_ptr<HIRProgram> lower_module(Program& program_node, Scope* module_scope,
                                             const std::vector<const Program*>& all_programs) try {
        sema.env.current_scope = module_scope != nullptr ? module_scope : sema.env.root_scope;

        register_generic_declarations(all_programs);
        cleanup.collect_destructors(all_programs);

        register_program_symbols(program_node);

        for (auto& statement : program_node.statements) {
            if (auto* lowered = lower_statement(*statement); lowered != nullptr) {
                program->statements.push_back(lowered);
            }
        }

        drain_specializations();

        for (const auto& entry : sema.env.root_scope->local_types()) {
            if (entry.second != nullptr) {
                program->referenced_types.push_back(entry.second);
            }
        }

        return diagnostics.has_errors() ? nullptr : program;
    } catch (const LoweringFailure& failure) {
        diagnostics.add_error(failure.span, failure.message);
        return nullptr;
    }

    std::shared_ptr<HIRProgram> lower(Program& program_node) {
        std::vector<const Program*> programs{&program_node};
        return lower_module(program_node, sema.env.root_scope, programs);
    }
};
