module;

#include <string>
#include <utility>
#include <vector>

export module zep.frontend.sema.type.builder;

import zep.frontend.sema.type;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.common.context;
import zep.frontend.sema.context;
import zep.frontend.sema.type.resolver;
import zep.frontend.sema.env;

export class TypeBuilder {
  private:
    Visitor<void>& visitor;

    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;

  public:
    explicit TypeBuilder(Context& context, SemaContext& sema, TypeResolver& resolver,
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
            prototype.receiver_kind == FunctionPrototype::ReceiverKind::Type::MutBorrow;
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

        std::vector<const InterfaceType*> interfaces;
        interfaces.reserve(node.inheritance.size());

        auto inherited_method_count = std::size_t{0};
        for (auto* inherited : node.inheritance) {
            visitor.visit(*inherited);

            const auto* resolved = resolver.resolve_type(inherited->type);
            const auto* interface_type =
                resolved != nullptr ? resolved->as<InterfaceType>() : nullptr;

            if (interface_type == nullptr) {
                context.diagnostics.add_error(inherited->span,
                                              "interface inheritance entry must be an interface");
                continue;
            }

            interfaces.push_back(interface_type);
            inherited_method_count += interface_type->methods.size();
        }

        std::vector<MethodType> methods;
        methods.reserve(inherited_method_count + node.methods.size());

        for (const auto* interface_type : interfaces) {
            for (const auto& method : interface_type->methods) {
                methods.emplace_back(method.name, method.type, methods.size());
            }
        }

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
        std::vector<const InterfaceType*> interfaces;
        interfaces.reserve(node.inheritance.size());
        auto inherited_method_count = std::size_t{0};

        if (node.backing_type != nullptr) {
            backing_type = resolver.resolve_type(node.backing_type->type);
        }

        for (auto* inherited : node.inheritance) {
            visitor.visit(*inherited);
            const auto* resolved = resolver.resolve_type(inherited->type);

            if (const auto* interface_type =
                    resolved != nullptr ? resolved->as<InterfaceType>() : nullptr;
                interface_type != nullptr) {
                interfaces.push_back(interface_type);
                inherited_method_count += interface_type->methods.size();
                continue;
            }

            context.diagnostics.add_error(inherited->span,
                                          "enum inheritance entry must be an interface");
        }

        auto variant_types = build_enum_variants(node);

        std::vector<MethodType> methods;
        methods.reserve(inherited_method_count + node.methods.size());

        for (const auto* interface_type : interfaces) {
            for (const auto& method : interface_type->methods) {
                methods.emplace_back(method.name, method.type, methods.size());
            }
        }

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
