module;

#include <string>
#include <utility>
#include <variant>
#include <vector>

export module zep.frontend.sema.declaration.function;

import zep.common.context;
import zep.frontend.node;
import zep.frontend.sema.scopes.environment;
import zep.frontend.node.program;
import zep.frontend.sema.context;
import zep.frontend.kind;
import zep.frontend.sema.declaration.attribute;
import zep.frontend.sema.types.compatibility;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.frontend.sema.types;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.lifecycle;

export class DeclarationTypeBuilder {
  private:
    Visitor<void>& visitor;

    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;

    std::vector<const InterfaceType*> build_interfaces(const std::vector<TypeExpression*>& inheritance,
                                                       const std::string& owner) {
        std::vector<const InterfaceType*> interfaces;
        interfaces.reserve(inheritance.size());

        for (auto* inherited : inheritance) {
            visitor.visit(*inherited);
            const auto* resolved = resolver.resolve_type(inherited->type);
            const auto* interface = resolved != nullptr ? resolved->as<InterfaceType>() : nullptr;

            if (interface != nullptr) {
                interfaces.push_back(interface);
            } else {
                context.diagnostics.add_error(inherited->span,
                                              owner + " inheritance entry must be an interface");
            }
        }

        return interfaces;
    }

    static std::vector<MethodType> inherit_methods(const std::vector<const InterfaceType*>& interfaces,
                                                  std::size_t own_count) {
        auto count = own_count;

        for (const auto* interface : interfaces) {
            count += interface->methods.size();
        }

        std::vector<MethodType> methods;
        methods.reserve(count);

        for (const auto* interface : interfaces) {
            for (const auto& method : interface->methods) {
                methods.emplace_back(method.name, method.type, methods.size());
            }
        }

        return methods;
    }

  public:
    explicit DeclarationTypeBuilder(Context& context, SemaContext& sema, TypeResolver& resolver,
                                    Visitor<void>& visitor)
        : visitor(visitor), context(context), sema(sema), resolver(resolver) {}

    std::vector<GenericParameterType>
    build_generic_parameter_types(std::vector<GenericParameter*>& generic_parameters) {
        std::vector<GenericParameterType> result;
        result.reserve(generic_parameters.size());

        for (auto* generic_parameter : generic_parameters) {
            if (generic_parameter->is_const()) {
                visitor.visit(*generic_parameter->value_type);
                result.emplace_back(
                    GenericParameterType::Kind::Type::Const, generic_parameter->name,
                    resolver.resolve_type(generic_parameter->value_type->type), generic_parameter);
                continue;
            }

            const Type* constraint = nullptr;
            if (generic_parameter->constraint != nullptr) {
                visitor.visit(*generic_parameter->constraint);
                constraint = generic_parameter->constraint->type;
            }

            result.emplace_back(generic_parameter->name, constraint, generic_parameter);
        }

        return result;
    }

    const Type* resolve_parameter_type(Parameter& parameter,
                                       const FunctionType* target_function_type = nullptr,
                                       std::size_t index = 0) {
        if (parameter.type != nullptr) {
            visitor.visit(*parameter.type);
            return resolver.resolve_type(parameter.type->type);
        }

        if (target_function_type != nullptr && index < target_function_type->parameters.size()) {
            return resolver.resolve_type(target_function_type->parameters[index].type);
        }

        return nullptr;
    }

    std::vector<ParameterType>
    build_parameter_types(std::vector<Parameter*>& parameters,
                          const FunctionType* target_function_type = nullptr) {
        std::vector<ParameterType> parameter_types;
        parameter_types.reserve(parameters.size());

        for (std::size_t index = 0; index < parameters.size(); ++index) {
            auto* parameter = parameters[index];
            const auto* parameter_type =
                resolve_parameter_type(*parameter, target_function_type, index);

            parameter_types.emplace_back(parameter->name, parameter_type);
        }

        return parameter_types;
    }

    void add_receiver_parameter(FunctionPrototype& prototype,
                                std::vector<ParameterType>& parameter_types,
                                const Type* self_type) {
        if (prototype.receiver_kind == FunctionPrototype::ReceiverKind::Type::None ||
            self_type == nullptr) {
            return;
        }

        const auto is_mutable =
            prototype.receiver_kind == FunctionPrototype::ReceiverKind::Type::MutBorrow ||
            prototype.receiver_kind == FunctionPrototype::ReceiverKind::Type::Move;
        const auto* receiver_type = sema.types.create<PointerType>(self_type, is_mutable);
        parameter_types.insert(parameter_types.begin(), ParameterType("self", receiver_type));
    }

    const Type* build_struct_type(StructDeclaration& node) {
        auto generic_parameter_types = build_generic_parameter_types(node.generic_parameters);

        auto substitution_scope = resolver.create_substitution_scope();
        resolver.bind_generic_parameters(generic_parameter_types, true);
        const auto* self_type =
            sema.types.create<NamedType>(node.name, std::vector<GenericArgumentType>());
        resolver.bind_type_parameter("Self", self_type);

        const StructType* base_type = nullptr;
        std::vector<const InterfaceType*> interfaces;
        interfaces.reserve(node.inheritance.size());

        for (auto* inherited : node.inheritance) {
            visitor.visit(*inherited);
            const auto* resolved = resolver.resolve_type(inherited->type);

            if (const auto* struct_type =
                    resolved != nullptr ? resolved->as<StructType>() : nullptr;
                struct_type != nullptr) {
                if (base_type != nullptr) {
                    context.diagnostics.add_error(inherited->span,
                                                  std::string("struct '") + node.name +
                                                      "' can only inherit one base struct");
                    continue;
                }

                base_type = struct_type;
                continue;
            }

            if (const auto* interface_type =
                    resolved != nullptr ? resolved->as<InterfaceType>() : nullptr;
                interface_type != nullptr) {
                interfaces.push_back(interface_type);
                continue;
            }

            context.diagnostics.add_error(inherited->span,
                                          "inheritance entry must be a struct or interface");
        }

        std::vector<FieldType> field_types;
        field_types.reserve((base_type != nullptr ? base_type->fields.size() : 0) +
                            node.fields.size());

        if (base_type != nullptr) {
            for (const auto& field : base_type->fields) {
                field_types.emplace_back(field.name, field.type, field.visibility);
            }
        }

        for (auto* field : node.fields) {
            visitor.visit(*field);
            field_types.emplace_back(field->name, field->type->type, field->visibility);
        }

        return sema.types.create<StructType>(
            node.name, std::move(generic_parameter_types), std::move(field_types),
            std::vector<GenericArgumentType>(), base_type, std::move(interfaces));
    }

    const Type* build_interface_type(InterfaceDeclaration& node) {
        auto generic_parameter_types = build_generic_parameter_types(node.generic_parameters);

        auto substitution_scope = resolver.create_substitution_scope();
        resolver.bind_generic_parameters(generic_parameter_types, true);
        const auto* self_type =
            sema.types.create<NamedType>("Self", std::vector<GenericArgumentType>());
        resolver.bind_type_parameter("Self", self_type);

        auto interfaces = build_interfaces(node.inheritance, "interface");
        auto methods = inherit_methods(interfaces, node.methods.size());

        for (std::size_t index = 0; index < node.methods.size(); ++index) {
            auto* method = node.methods[index];
            const auto* method_type =
                build_function_type(*method, generic_parameter_types, self_type);
            methods.emplace_back(method->name, method_type, methods.size());
        }

        return sema.types.create<InterfaceType>(node.name, std::move(generic_parameter_types),
                                                std::move(methods), std::move(interfaces));
    }

    std::vector<EnumVariantType> build_enum_variants(EnumDeclaration& node) {
        std::vector<EnumVariantType> variant_types;
        variant_types.reserve(node.variants.size());

        for (std::size_t variant_index = 0; variant_index < node.variants.size(); ++variant_index) {
            auto* variant = node.variants[variant_index];

            std::vector<FieldType> field_types;
            field_types.reserve(variant->fields.size());

            for (auto* field : variant->fields) {
                visitor.visit(*field);
                field_types.emplace_back(field->name, field->type->type);
            }

            variant_types.emplace_back(variant->name, variant_index, std::move(field_types));
        }

        return variant_types;
    }

    Type* build_enum_type(EnumDeclaration& node) {
        auto generic_parameter_types = build_generic_parameter_types(node.generic_parameters);

        auto substitution_scope = resolver.create_substitution_scope();
        resolver.bind_generic_parameters(generic_parameter_types, true);
        const auto* self_type =
            sema.types.create<NamedType>(node.name, std::vector<GenericArgumentType>());
        resolver.bind_type_parameter("Self", self_type);

        const Type* backing_type = nullptr;

        if (node.backing_type != nullptr) {
            backing_type = resolver.resolve_type(node.backing_type->type);
        }

        auto interfaces = build_interfaces(node.inheritance, "enum");
        auto variant_types = build_enum_variants(node);
        auto methods = inherit_methods(interfaces, node.methods.size());

        for (std::size_t index = 0; index < node.methods.size(); ++index) {
            auto* method = node.methods[index];
            const auto* method_type =
                build_function_type(*method->prototype, generic_parameter_types, self_type);
            methods.emplace_back(method->prototype->name, method_type, methods.size());
        }

        return sema.types.create<EnumType>(
            node.name, std::move(generic_parameter_types), std::move(variant_types), backing_type,
            std::vector<GenericArgumentType>(), std::move(interfaces), std::move(methods));
    }

    const FunctionType*
    build_function_type(FunctionPrototype& prototype,
                        const std::vector<GenericParameterType>& parent_generic_parameters = {},
                        const Type* self_type = nullptr) {
        auto generic_parameter_types = build_generic_parameter_types(prototype.generic_parameters);
        generic_parameter_types.insert(generic_parameter_types.begin(),
                                       parent_generic_parameters.begin(),
                                       parent_generic_parameters.end());

        auto substitution_scope = resolver.create_substitution_scope();
        resolver.bind_generic_parameters(parent_generic_parameters, true);
        resolver.bind_generic_parameters(generic_parameter_types, true);
        if (self_type != nullptr) {
            resolver.bind_type_parameter("Self", self_type);
        }

        auto parameter_types = build_parameter_types(prototype.parameters);
        add_receiver_parameter(prototype, parameter_types, self_type);

        visitor.visit(*prototype.return_type);

        return sema.types.create<FunctionType>(
            prototype.name, prototype.return_type->type, std::move(parameter_types),
            std::move(generic_parameter_types), prototype.is_variadic);
    }
};
export class FunctionDeclarationChecker {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    DeclarationTypeBuilder& builder;
    Compatibility& compatibility;

    const FunctionType* resolve_with_self_type(const FunctionType* type, const Type* self_type) {
        auto scope = resolver.create_substitution_scope();
        const auto* nominal = self_type != nullptr ? self_type->as_nominal() : nullptr;
        if (nominal != nullptr) {
            for (const auto& parameter : nominal->generic_parameters) {
                resolver.bind_generic_parameter(parameter, true);
            }
        }

        resolver.bind_type_parameter("Self", self_type);

        const auto* resolved = resolver.resolve_type(type);
        return resolved != nullptr ? resolved->as<FunctionType>() : nullptr;
    }

    bool matches_extension_owner(const Type* receiver_type, const Type* owner_type) {
        if (compatibility.accepts(receiver_type, owner_type)) {
            return true;
        }

        const auto* receiver_nominal =
            receiver_type != nullptr ? receiver_type->as_nominal() : nullptr;
        const auto* owner_nominal = owner_type != nullptr ? owner_type->as_nominal() : nullptr;

        return receiver_nominal != nullptr && owner_nominal != nullptr &&
               receiver_nominal->same_nominal(*owner_nominal);
    }

    FunctionSymbol::Role classify_function(FunctionDeclaration& node,
                                                  const FunctionType& type) {
        if (node.is_extension) {
            return FunctionSymbol::Role(node.is_static ? MethodKind::Type::StaticExtension
                                                       : MethodKind::Type::InstanceExtension);
        }

        const auto kind = node.role();
        if (kind != FunctionSymbol::Role(ConstructorKind::Type::Value) ||
            node.prototype->parameters.size() != 1 || type.parameters.size() != 1) {
            return kind;
        }

        const auto* parameter = resolver.resolve_type(type.parameters.front().type);
        const auto* pointer = parameter != nullptr ? parameter->as<PointerType>() : nullptr;
        const auto* owner = sema.env.current_scope->lookup_type(node.parent);
        const auto* owner_nominal = owner != nullptr ? owner->type->as_nominal() : nullptr;
        const auto* resolved_source =
            pointer != nullptr ? resolver.resolve_type(pointer->element) : nullptr;
        const auto* source = resolved_source != nullptr ? resolved_source->as_nominal() : nullptr;

        if (pointer != nullptr && owner_nominal != nullptr && source != nullptr &&
            owner_nominal->same_nominal(*source)) {
            if (pointer->reference_kind == PointerType::ReferenceKind::Type::Move) {
                return FunctionSymbol::Role(ConstructorKind::Type::Move);
            }

            if (pointer->reference_kind == PointerType::ReferenceKind::Type::Borrow &&
                !pointer->is_mutable) {
                return FunctionSymbol::Role(ConstructorKind::Type::Copy);
            }
        }
        return kind;
    }

  public:
    FunctionDeclarationChecker(Context& context, SemaContext& sema, TypeResolver& resolver,
                               DeclarationTypeBuilder& builder, Compatibility& compatibility)
        : context(context), sema(sema), resolver(resolver), builder(builder),
          compatibility(compatibility) {}

    bool method_signature_matches(const FunctionType* expected, const FunctionType* actual,
                                  const Type* self_type) {
        const auto* resolved_expected = resolve_with_self_type(expected, self_type);
        const auto* resolved_actual = resolve_with_self_type(actual, self_type);

        if (resolved_expected == nullptr || resolved_actual == nullptr) {
            return false;
        }

        if (resolved_expected->return_type == nullptr || resolved_actual->return_type == nullptr) {
            return false;
        }

        if (!resolved_expected->return_type->accepts(resolved_actual->return_type)) {
            return false;
        }

        if (resolved_expected->parameters.size() != resolved_actual->parameters.size()) {
            return false;
        }

        for (std::size_t i = 0; i < resolved_expected->parameters.size(); ++i) {
            const auto* expected_param = resolved_expected->parameters[i].type;
            const auto* actual_param = resolved_actual->parameters[i].type;

            if (expected_param == nullptr || actual_param == nullptr) {
                return false;
            }

            const auto* expected_pointer = expected_param->as<PointerType>();
            const auto* actual_pointer = actual_param->as<PointerType>();

            if (expected_pointer != nullptr && actual_pointer != nullptr && i == 0) {
                const auto* expected_element = resolver.resolve_type(expected_pointer->element);
                const auto* actual_element = resolver.resolve_type(actual_pointer->element);
                const auto* expected_struct =
                    expected_element != nullptr ? expected_element->as<StructType>() : nullptr;
                const auto* actual_struct =
                    actual_element != nullptr ? actual_element->as<StructType>() : nullptr;

                if (expected_struct != nullptr && actual_struct != nullptr &&
                    (expected_struct->inherits_from(actual_struct) ||
                     actual_struct->inherits_from(expected_struct) ||
                     expected_struct->same_nominal(*actual_struct))) {
                    continue;
                }
            }

            if (!expected_param->accepts(actual_param)) {
                return false;
            }
        }

        return true;
    }

    bool validate_function_declaration(FunctionDeclaration& node, const FunctionSymbol* symbol) {
        if (symbol == nullptr || symbol->signature() == nullptr) {
            return false;
        }

        const auto* owner_symbol =
            node.is_member() ? sema.env.current_scope->lookup_type(node.parent) : nullptr;
        const auto* owner_type = owner_symbol != nullptr ? owner_symbol->type : nullptr;
        const auto* owner_struct_type =
            owner_type != nullptr ? owner_type->as<StructType>() : nullptr;

        const auto* return_type = resolver.resolve_type(symbol->signature()->return_type);

        if (node.is_extension) {
            const auto expected_kind = FunctionSymbol::Role(node.is_static ? MethodKind::Type::Static
                                                                           : MethodKind::Type::Instance);
            if (node.is_override || node.role() != expected_kind) {
                context.diagnostics.add_error(node.span, "invalid extension method declaration");
                return false;
            }
        }

        for (auto* parameter : node.prototype->parameters) {
            if (parameter->name == "self") {
                context.diagnostics.add_error(parameter->span, "methods use implicit self");
                return false;
            }
        }

        if (node.is_static &&
            node.prototype->receiver_kind != FunctionPrototype::ReceiverKind::Type::None) {
            context.diagnostics.add_error(node.span, "static method cannot be declared 'mut'");
            return false;
        }

        if (symbol->instance()) {
            if (node.prototype->receiver_kind == FunctionPrototype::ReceiverKind::Type::None) {
                context.diagnostics.add_error(node.span, "instance method '" +
                                                             node.prototype->name +
                                                             "' must declare an implicit receiver");
                return false;
            }

            if (node.is_extension && owner_type != nullptr) {
                const auto* self_type =
                    resolver.resolve_type(symbol->signature()->parameters.front().type);
                const auto* receiver_type = self_type;

                if (const auto* pointer_type =
                        self_type != nullptr ? self_type->as<PointerType>() : nullptr) {
                    receiver_type = resolver.resolve_type(pointer_type->element);
                }

                if (receiver_type == nullptr ||
                    !matches_extension_owner(receiver_type, owner_type)) {
                    context.diagnostics.add_error(
                        node.span, "extension method receiver must match '" + node.parent + "'");
                    return false;
                }
            }

            return true;
        }

        if (symbol->static_method() || symbol->has_role(CallableKind::Type::Function)) {
            return true;
        }

        if (symbol->has_role(ConstructorKind::Type::Value)) {
            if (node.is_static) {
                context.diagnostics.add_error(node.span, "constructor cannot be declared static");
                return false;
            }

            if (owner_struct_type == nullptr || return_type == nullptr ||
                !compatibility.accepts(return_type, owner_struct_type)) {
                context.diagnostics.add_error(node.span, "constructor '" + node.prototype->name +
                                                             "' must return '" + node.parent + "'");
                return false;
            }

            return true;
        }

        if (symbol->has_role(ConstructorKind::Type::Copy)) {
            if (node.is_static || owner_struct_type == nullptr || return_type == nullptr ||
                !compatibility.accepts(return_type, owner_struct_type)) {
                context.diagnostics.add_error(node.span, "invalid copy constructor for '" +
                                                             node.parent + "'");
                return false;
            }

            return true;
        }

        if (symbol->has_role(ConstructorKind::Type::Move)) {
            return !node.is_static && owner_struct_type != nullptr && return_type != nullptr &&
                   compatibility.accepts(return_type, owner_struct_type);
        }

        if (symbol->has_role(CallableKind::Type::Destructor)) {
            if (node.is_static) {
                context.diagnostics.add_error(node.span, "destructor cannot be declared static");
                return false;
            }

            if (return_type == nullptr || !return_type->is<VoidType>()) {
                context.diagnostics.add_error(node.span, "destructor '" + node.prototype->name +
                                                             "' must return void");
                return false;
            }

            if (!node.prototype->parameters.empty()) {
                context.diagnostics.add_error(node.span, "destructor '" + node.prototype->name +
                                                             "' must not declare parameters");
                return false;
            }

            const auto* self_type =
                resolver.resolve_type(symbol->signature()->parameters.front().type);
            const auto* pointer_type =
                self_type != nullptr ? self_type->as<PointerType>() : nullptr;
            const auto* element_type =
                pointer_type != nullptr ? pointer_type->element->as<StructType>() : nullptr;

            if (pointer_type == nullptr || !pointer_type->is_mutable || element_type == nullptr ||
                element_type->name != node.parent) {
                context.diagnostics.add_error(node.span, "destructor receiver must be '*mut " +
                                                             node.parent + "'");
                return false;
            }

            const auto* owner = sema.env.current_scope->lookup_type(node.parent);
            auto* member_scope = Scope::members(owner != nullptr ? owner->type : nullptr);
            if (member_scope != nullptr) {
                const auto* overloads =
                    member_scope->find_local_function_overloads(node.prototype->name);
                if (overloads != nullptr && overloads->size() > 1) {
                    context.diagnostics.add_error(node.span, "duplicate destructor for struct '" +
                                                                 node.parent + "'");
                    return false;
                }
            }

            return true;
        }

        return true;
    }

    const FunctionSymbol* declare_function(FunctionDeclaration& node) {
        if (node.function_symbol != nullptr) {
            return node.function_symbol;
        }

        auto attribute_infos = AttributeResolver::convert(node.attributes);
        AttributeResolver::validate(
            attribute_infos,
            {sema, context, node.span, node.prototype->name, node.prototype->generic_parameters});

        if (node.type == nullptr) {
            std::vector<GenericParameterType> parent_generics;
            const Type* self_type = nullptr;
            if (node.is_member() || node.is_extension) {
                const auto* parent_symbol = sema.env.current_scope->lookup_type(node.parent);
                self_type = parent_symbol != nullptr ? parent_symbol->type : nullptr;
                const auto* nominal = self_type != nullptr ? self_type->as_nominal() : nullptr;
                if (nominal != nullptr && node.prototype->generic_parameters.empty()) {
                    parent_generics = nominal->generic_parameters;
                }
            }

            node.type = builder.build_function_type(*node.prototype, parent_generics, self_type);
        }

        const auto* type = node.type->as<FunctionType>();
        if (type == nullptr) {
            return nullptr;
        }

        auto* target_scope = sema.env.current_scope;
        if (node.is_member()) {
            const auto* owner = sema.env.current_scope->lookup_type(node.parent);
            if (auto* members = Scope::members(owner != nullptr ? owner->type : nullptr);
                members != nullptr) {
                target_scope = members;
            }
        }

        const auto* existing_overloads =
            target_scope->find_local_function_overloads(node.prototype->name);
        if (existing_overloads != nullptr) {
            for (auto* symbol : *existing_overloads) {
                const auto* function_type = symbol->type->as<FunctionType>();
                if (function_type == nullptr) {
                    continue;
                }

                if (function_type->conflicts_with(*type)) {
                    context.diagnostics.add_error(node.span, "redefinition of function '" +
                                                                 node.prototype->name + "'");
                    return nullptr;
                }
            }
        }

        const auto role = classify_function(node, *type);
        if (node.is_defaulted && (role == FunctionSymbol::Role(ConstructorKind::Type::Copy) ||
                                 role == FunctionSymbol::Role(ConstructorKind::Type::Move))) {
            attribute_infos.emplace_back(".defaulted_constructor", std::vector<std::string>());
        }
        if (node.prototype->receiver_kind == FunctionPrototype::ReceiverKind::Type::Move) {
            attribute_infos.emplace_back(".move_receiver", std::vector<std::string>());
        }
        auto* symbol = sema.env.symbols.create<FunctionSymbol>(
            node.prototype->name, node.span, node.visibility, Linkage::Type::Internal, type,
            role, node.parent, std::move(attribute_infos), Abi::Type::Language, node.is_deleted);

        target_scope->define_function(node.prototype->name, symbol);

        node.function_symbol = symbol;

        if (!node.parent.empty()) {
            const auto* owner = sema.env.current_scope->lookup_type(node.parent);
            const auto* nominal = owner != nullptr ? owner->type->as_nominal() : nullptr;
            if (nominal != nullptr && role == FunctionSymbol::Role(ConstructorKind::Type::Copy)) {
                sema.lifecycle.register_function(nominal, Operation::Type::Copy, symbol);
            } else if (nominal != nullptr && role == FunctionSymbol::Role(ConstructorKind::Type::Move)) {
                sema.lifecycle.register_function(nominal, Operation::Type::Move, symbol);
            } else if (nominal != nullptr &&
                       role == FunctionSymbol::Role(CallableKind::Type::Destructor)) {
                sema.lifecycle.register_function(nominal, Operation::Type::Destroy, symbol);
            }
        }

        return symbol;
    }

    void declare_extern_function(ExternFunctionDeclaration& node) {
        if (node.function_symbol != nullptr) {
            return;
        }

        auto attribute_infos = AttributeResolver::convert(node.attributes);
        AttributeResolver::validate(
            attribute_infos,
            {sema, context, node.span, node.prototype->name, node.prototype->generic_parameters});

        if (node.type != nullptr) {
            for (auto* symbol :
                 sema.env.current_scope->lookup_function_overloads(node.prototype->name)) {
                if (symbol->type == node.type) {
                    return;
                }
            }
        }

        const auto* type = builder.build_function_type(*node.prototype);
        node.type = type;

        auto* symbol = sema.env.symbols.create<FunctionSymbol>(
            node.prototype->name, node.span, node.visibility, Linkage::Type::External, type,
            FunctionSymbol::Role(CallableKind::Type::Function), std::string{}, std::move(attribute_infos),
            Abi::Type::C);
        sema.env.current_scope->define_function(node.prototype->name, symbol);
        node.function_symbol = symbol;
    }
};
