module;

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

export module zep.frontend.sema.declaration.struct_checker;

import zep.common.context;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.kind;
import zep.frontend.sema.resolver.attribute;
import zep.frontend.sema.resolver.facade;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.type.builder;
import zep.frontend.sema.declaration.function;

export class StructDeclarationChecker {
  private:
    Context& context;
    SemaContext& sema;
    TypeBuilder& builder;
    FacadeResolver& facades;
    FunctionDeclarationChecker& functions;
    std::function<void(EnumDeclaration&)> declare_enum_type_fn;

    void register_type(const std::string& name, Span span, Visibility::Type visibility,
                       const Type* type, const std::vector<GenericParameter*>& generic_parameters,
                       const std::vector<Attribute*>& attributes, Scope* type_scope = nullptr) {
        auto attribute_infos = AttributeResolver::convert(attributes);
        AttributeResolver::validate(attribute_infos,
                                    {sema, context, span, name, generic_parameters});

        auto* symbol = sema.env.symbols.create<TypeSymbol>(name, span, visibility, type,
                                                           std::move(attribute_infos));
        if (!sema.env.current_scope->define_type(name, symbol)) {
            context.diagnostics.add_error(span, "redefinition of type '" + name + "'");
        }

        if (type_scope != nullptr) {
            symbol->member_scope = type_scope;
            if (auto* nominal =
                    const_cast<NominalType*>(type != nullptr ? type->as_nominal() : nullptr);
                nominal != nullptr) {
                nominal->member_scope = type_scope;
            }
        }
    }

    const MethodType* resolve_method(const StructType* struct_type, const std::string& method_name,
                                     const FunctionType* requirement) {
        if (struct_type == nullptr) {
            return nullptr;
        }

        for (const auto& method : struct_type->methods) {
            if (method.name == method_name &&
                functions.method_signature_matches(requirement, method.type, struct_type)) {
                return &method;
            }
        }

        return resolve_method(struct_type->base_type, method_name, requirement);
    }

    bool has_method_in_hierarchy(const StructType* struct_type, const std::string& method_name,
                                 const FunctionType* signature) {
        return resolve_method(struct_type, method_name, signature) != nullptr;
    }

    bool method_satisfies_requirement(const StructType& struct_type, const FunctionType* actual,
                                      const std::string& method_name) {
        for (const auto* interface_type : struct_type.interfaces) {
            if (interface_type == nullptr) {
                continue;
            }

            const auto* requirement = interface_type->find_method(method_name);
            if (requirement != nullptr &&
                functions.method_signature_matches(requirement->type, actual, &struct_type)) {
                return true;
            }
        }

        return false;
    }

    void validate_interface_conformance(StructDeclaration& node, StructType& struct_type) {
        for (const auto* interface_type : struct_type.interfaces) {
            if (interface_type == nullptr) {
                continue;
            }

            for (const auto& requirement : interface_type->methods) {
                if (resolve_method(&struct_type, requirement.name, requirement.type) != nullptr) {
                    continue;
                }

                context.diagnostics.add_error(
                    node.span, "struct '" + node.name + "' does not implement method '" +
                                   requirement.name + "' required by interface '" +
                                   interface_type->name + "'");
            }
        }
    }

    void validate_interface_method(FunctionPrototype& method) {
        if (method.receiver_kind == FunctionPrototype::ReceiverKind::Type::None) {
            context.diagnostics.add_error(method.span, "interface method '" + method.name +
                                                           "' must have a receiver");
        }

        for (auto* parameter : method.parameters) {
            if (parameter->name == "self") {
                context.diagnostics.add_error(parameter->span,
                                              "interface methods use implicit self");
            }
        }
    }

    void validate_overrides(StructDeclaration& node, StructType& struct_type) {
        for (auto* method : node.methods) {
            if (method->function_symbol == nullptr ||
                method->function_symbol->function_type == nullptr ||
                method->function_symbol->callable_kind !=
                    FunctionSymbol::Kind::Type::InstanceMethod) {
                if (method->is_override) {
                    context.diagnostics.add_error(
                        method->span, "'override' can only be used on instance methods");
                }
                continue;
            }

            const auto matches_base =
                has_method_in_hierarchy(struct_type.base_type, method->prototype->name,
                                        method->function_symbol->function_type);
            const auto matches_interface = method_satisfies_requirement(
                struct_type, method->function_symbol->function_type, method->prototype->name);

            if (method->is_override && !matches_base && !matches_interface) {
                context.diagnostics.add_error(method->span, "method '" + method->prototype->name +
                                                                "' does not override anything");
            }

            if (!method->is_override && (matches_base || matches_interface)) {
                context.diagnostics.add_error(method->span, "method '" + method->prototype->name +
                                                                "' must be declared override");
            }
        }
    }

    void validate_struct_declaration(StructDeclaration& node) {
        std::unordered_set<std::string> field_names;
        field_names.reserve(node.fields.size());

        for (auto* field : node.fields) {
            if (field_names.contains(field->name)) {
                context.diagnostics.add_error(field->span, "duplicate field '" + field->name +
                                                               "' in struct '" + node.name + "'");
            }
            field_names.insert(field->name);
        }
    }

  public:
    StructDeclarationChecker(Context& context, SemaContext& sema, TypeBuilder& builder,
                             FacadeResolver& facades, FunctionDeclarationChecker& functions,
                             std::function<void(EnumDeclaration&)> declare_enum_type_fn)
        : context(context), sema(sema), builder(builder), facades(facades), functions(functions),
          declare_enum_type_fn(std::move(declare_enum_type_fn)) {}

    void declare_interface_type(InterfaceDeclaration& node) {
        if (node.type != nullptr) {
            return;
        }

        const auto* type = builder.build_interface_type(node);
        node.type = type;

        auto* type_scope = sema.env.scopes.create<Scope>(Scope::Kind::Type::Interface, node.name,
                                                         sema.env.current_scope);

        register_type(node.name, node.span, node.visibility, type, node.generic_parameters,
                      node.attributes, type_scope);

        const auto* interface_type = type->as<InterfaceType>();
        if (interface_type == nullptr) {
            return;
        }

        const auto method_offset = interface_type->methods.size() >= node.methods.size()
                                       ? interface_type->methods.size() - node.methods.size()
                                       : 0;

        for (std::size_t index = 0; index < node.methods.size(); ++index) {
            auto* method = node.methods[index];
            validate_interface_method(*method);

            auto attribute_infos = std::vector<AttributeInfo>{};

            const auto method_index = method_offset + index;
            const auto* method_type = method_index < interface_type->methods.size()
                                          ? interface_type->methods[method_index].type
                                          : nullptr;
            if (method_type == nullptr) {
                continue;
            }

            auto* symbol = sema.env.symbols.create<FunctionSymbol>(
                method->name, method->span, Visibility::Type::Public, Linkage::Type::Internal,
                method_type, FunctionSymbol::Kind::Type::InstanceMethod, node.name,
                std::move(attribute_infos), false, Abi::Type::Language);

            type_scope->define_function(method->name, symbol,
                                        sema.env.overloads.create<OverloadSet>());
        }
    }

    void declare_struct_type(StructDeclaration& node) {
        if (node.type != nullptr) {
            return;
        }

        validate_struct_declaration(node);

        const auto* type = builder.build_struct_type(node);
        node.type = type;

        auto* type_scope = sema.env.scopes.create<Scope>(Scope::Kind::Type::Struct, node.name,
                                                         sema.env.current_scope);

        register_type(node.name, node.span, node.visibility, type, node.generic_parameters,
                      node.attributes, type_scope);

        auto* struct_type = const_cast<StructType*>(type->as<StructType>());
        if (struct_type != nullptr) {
            for (const auto& field : struct_type->fields) {
                const auto* source_field = static_cast<Field*>(nullptr);
                for (auto* node_field : node.fields) {
                    if (node_field->name == field.name) {
                        source_field = node_field;
                        break;
                    }
                }

                auto attributes = source_field != nullptr
                                      ? AttributeResolver::convert(source_field->attributes)
                                      : std::vector<AttributeInfo>{};

                if (source_field != nullptr) {
                    AttributeResolver::validate(
                        attributes, {sema, context, source_field->span, field.name, {}});
                }

                if (!type_scope->define_var(field.name, sema.env.symbols.create<VariableSymbol>(
                                                            field.name, node.span, field.visibility,
                                                            StorageKind::Type::Var, field.type,
                                                            std::move(attributes)))) {
                    context.diagnostics.add_error(node.span,
                                                  "redefinition of field '" + field.name + "'");
                }
            }

            facades.register_struct(*struct_type, node.visibility);
        }

        for (auto* method : node.methods) {
            functions.declare_function(*method);
        }

        if (struct_type != nullptr) {
            struct_type->methods.clear();
            struct_type->methods.reserve(node.methods.size());

            std::size_t method_index = 0;
            for (auto* method : node.methods) {
                if (method->function_symbol == nullptr ||
                    method->function_symbol->function_type == nullptr ||
                    method->function_symbol->callable_kind !=
                        FunctionSymbol::Kind::Type::InstanceMethod) {
                    continue;
                }

                struct_type->methods.emplace_back(
                    method->prototype->name, method->function_symbol->function_type, method_index);
                ++method_index;
            }

            validate_overrides(node, *struct_type);
            validate_interface_conformance(node, *struct_type);
        }

        for (auto* nested_struct : node.structs) {
            auto original_name = nested_struct->name;
            nested_struct->name = node.name + "::" + original_name;
            declare_struct_type(*nested_struct);
            nested_struct->name = original_name;
        }

        for (auto* nested_enum : node.enums) {
            auto original_name = nested_enum->name;
            nested_enum->name = node.name + "::" + original_name;
            if (declare_enum_type_fn) {
                declare_enum_type_fn(*nested_enum);
            }
            nested_enum->name = original_name;
        }
    }
};
