module;

#include <algorithm>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

export module zep.frontend.sema.declaration.enum_checker;

import zep.common.context;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.constant.environment;
import zep.frontend.sema.constant.evaluator;
import zep.frontend.sema.context;
import zep.frontend.sema.kind;
import zep.frontend.sema.resolver.attribute;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.type.builder;
import zep.frontend.sema.type.resolver;
import zep.frontend.sema.declaration.function;

export class EnumDeclarationChecker {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    TypeBuilder& builder;
    FunctionDeclarationChecker& functions;
    Visitor<void>& visitor;
    std::function<void(Expression&, const Type*)> check_expression;

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

    void resolve_enum_backing(EnumDeclaration& declaration) {
        if (declaration.backing_type == nullptr) {
            return;
        }

        visitor.visit(*declaration.backing_type);
        const auto* type = resolver.resolve_type(declaration.backing_type->type);

        if (type != nullptr && type->is<InterfaceType>()) {
            declaration.inheritance.insert(declaration.inheritance.begin(),
                                           declaration.backing_type);
            declaration.backing_type = nullptr;
            return;
        }

        if (type == nullptr ||
            !(type->is<IntegerType>() || type->is<BooleanType>() || type->is<CharType>() ||
              type->is<StringType>() || type->is<StructType>())) {
            context.diagnostics.add_error(
                declaration.backing_type->span,
                "enum backing type must be an integer, boolean, char, cstr, or struct type");
        }
    }

    bool check_discriminant_references(Expression& expression, const EnumDeclaration& declaration,
                                       std::size_t index) {
        const auto* identifier = expression.as<IdentifierExpression>();
        const auto* qualified = expression.as<QualifiedAccessExpression>();

        const auto name = identifier != nullptr ? identifier->name
                          : qualified != nullptr && qualified->parent == declaration.name
                              ? qualified->member
                              : std::string();

        if (!name.empty()) {
            for (auto other = index; other < declaration.variants.size(); ++other) {
                if (declaration.variants[other]->name == name) {
                    context.diagnostics.add_error(
                        expression.span,
                        other == index ? "cyclic enum discriminant reference to '" + name + "'"
                                       : "forward enum discriminant reference to '" + name + "'");
                    return false;
                }
            }
        }

        if (auto* binary = expression.as<BinaryExpression>(); binary != nullptr) {
            return check_discriminant_references(*binary->left, declaration, index) &&
                   check_discriminant_references(*binary->right, declaration, index);
        }

        if (auto* unary = expression.as<UnaryExpression>(); unary != nullptr) {
            return check_discriminant_references(*unary->operand, declaration, index);
        }

        if (auto* member = expression.as<MemberExpression>(); member != nullptr) {
            return check_discriminant_references(*member->value, declaration, index);
        }

        if (auto* call = expression.as<CallExpression>(); call != nullptr) {
            for (auto* argument : call->arguments) {
                if (!check_discriminant_references(*argument->value, declaration, index)) {
                    return false;
                }
            }
        }

        if (auto* aggregate = expression.as<StructLiteralExpression>(); aggregate != nullptr) {
            for (auto* field : aggregate->fields) {
                if (!check_discriminant_references(*field->value, declaration, index)) {
                    return false;
                }
            }
        }

        return true;
    }

    void evaluate_enum_discriminants(EnumDeclaration& declaration, EnumType& type,
                                     Scope* type_scope) {
        ScopeGuard scope(sema.env.current_scope, sema.env.scopes, Scope::Kind::Type::Block,
                         declaration.name);

        CompileTimeEnvironment enum_values;
        Evaluator evaluator(context.diagnostics, &sema.env, &enum_values);

        const auto* backing = type.backing_type != nullptr
                                  ? type.backing_type
                                  : sema.builtin_resolver.primitives.at("i32");

        std::optional<CompileTimeValue> previous;
        std::vector<CompileTimeValue> values;
        values.reserve(declaration.variants.size());

        for (std::size_t index = 0; index < declaration.variants.size(); ++index) {
            auto* variant = declaration.variants[index];
            std::optional<CompileTimeValue> value;

            if (type.backing_type == nullptr) {
                if (variant->value_expression != nullptr) {
                    context.diagnostics.add_error(
                        variant->span, "enum variant '" + variant->name +
                                           "' cannot have a value without a backing type");
                }

                value = CompileTimeValue::checked_integer(
                    context.diagnostics, static_cast<__int128>(index), backing, variant->span);
            } else if (variant->value_expression != nullptr) {
                if (!check_discriminant_references(*variant->value_expression, declaration,
                                                   index)) {
                    previous.reset();
                    continue;
                }

                check_expression(*variant->value_expression, backing);
                value = evaluator.evaluate(*variant->value_expression);

                if (value.has_value() && !backing->same(value->type)) {
                    if (backing->is<IntegerType>() && value->is_integer()) {
                        const auto numeric =
                            value->kind == CompileTimeValue::Kind::Type::SignedInteger
                                ? static_cast<__int128>(std::get<std::int64_t>(value->payload))
                                : static_cast<__int128>(std::get<std::uint64_t>(value->payload));
                        value = CompileTimeValue::checked_integer(
                            context.diagnostics, numeric, backing, variant->value_expression->span);
                    } else {
                        context.diagnostics.add_error(
                            variant->value_expression->span,
                            "backed enum variant value type mismatch: expected '" +
                                backing->to_string() + "'");
                        value.reset();
                    }
                }
            } else if (backing->is<IntegerType>()) {
                if (index == 0) {
                    value = CompileTimeValue::checked_integer(context.diagnostics, 0, backing,
                                                              variant->span);
                } else if (previous.has_value()) {
                    const auto numeric =
                        previous->kind == CompileTimeValue::Kind::Type::SignedInteger
                            ? static_cast<__int128>(std::get<std::int64_t>(previous->payload))
                            : static_cast<__int128>(std::get<std::uint64_t>(previous->payload));
                    value = CompileTimeValue::checked_integer(context.diagnostics, numeric + 1,
                                                              backing, variant->span, true);
                }
            } else {
                context.diagnostics.add_error(variant->span, "backed enum variant '" +
                                                                 variant->name +
                                                                 "' requires an explicit value");
            }

            previous = value;
            if (!value.has_value()) {
                continue;
            }

            if (std::ranges::find(values, *value) != values.end()) {
                context.diagnostics.add_error(variant->span, "duplicate enum discriminant for '" +
                                                                 variant->name + "'");
            }

            values.push_back(*value);
            type.variants[index].discriminant = *value;
            enum_values.bind(variant->name, GenericBinding(ConstBinding(*value)));

            auto* symbol = sema.env.symbols.create<VariableSymbol>(variant->name, variant->span,
                                                                   Visibility::Type::Private,
                                                                   StorageKind::Type::Var, backing);
            sema.env.current_scope->define_var(variant->name, symbol);
        }

        for (std::size_t i = 0; i < declaration.variants.size(); ++i) {
            auto* variant = declaration.variants[i];
            const auto* variant_type = type.find_variant(variant->name);

            if (variant_type == nullptr) {
                continue;
            }

            if (!type_scope->define_variant(variant->name,
                                            sema.env.symbols.create<EnumVariantSymbol>(
                                                variant->name, variant->span,
                                                Visibility::Type::Public, &type, variant_type))) {
                context.diagnostics.add_error(variant->span, "redefinition of enum variant '" +
                                                                 variant->name + "'");
            }
        }
    }

    void validate_enum_declaration(EnumDeclaration& node) {
        const auto has_backing_type = node.backing_type != nullptr;

        if (has_backing_type && !node.generic_parameters.empty()) {
            context.diagnostics.add_error(node.span,
                                          "backed enums do not support generic parameters in v1");
        }

        std::unordered_set<std::string> variant_names;
        variant_names.reserve(node.variants.size());

        for (auto* variant : node.variants) {
            if (variant_names.contains(variant->name)) {
                context.diagnostics.add_error(variant->span, "duplicate variant '" + variant->name +
                                                                 "' in enum '" + node.name + "'");
            }
            variant_names.insert(variant->name);

            if (has_backing_type && !variant->fields.empty()) {
                context.diagnostics.add_error(variant->span, "backed enum variant '" +
                                                                 variant->name +
                                                                 "' cannot have payload fields");
            }

            std::unordered_set<std::string> field_names;
            field_names.reserve(variant->fields.size());

            for (auto* field : variant->fields) {
                if (field_names.contains(field->name)) {
                    context.diagnostics.add_error(field->span, "duplicate field '" + field->name +
                                                                   "' in enum variant '" +
                                                                   variant->name + "'");
                }
                field_names.insert(field->name);
            }
        }

        for (auto* method : node.methods) {
            if (method->kind() == FunctionSymbol::Kind::Type::Constructor ||
                method->kind() == FunctionSymbol::Kind::Type::Destructor) {
                context.diagnostics.add_error(method->span,
                                              "enums do not support constructors or destructors");
            }
        }
    }

    void validate_interface_conformance(EnumDeclaration& node, EnumType& enum_type) {
        for (const auto* interface_type : enum_type.interfaces) {
            if (interface_type == nullptr) {
                continue;
            }

            for (const auto& requirement : interface_type->methods) {
                bool found = false;
                for (const auto& method : enum_type.methods) {
                    if (method.name == requirement.name &&
                        functions.method_signature_matches(requirement.type, method.type,
                                                           &enum_type)) {
                        found = true;
                        break;
                    }
                }

                if (found) {
                    continue;
                }

                context.diagnostics.add_error(
                    node.span, "enum '" + node.name + "' does not implement method '" +
                                   requirement.name + "' required by interface '" +
                                   interface_type->name + "'");
            }
        }
    }

  public:
    EnumDeclarationChecker(Context& context, SemaContext& sema, TypeResolver& resolver,
                           TypeBuilder& builder, FunctionDeclarationChecker& functions,
                           Visitor<void>& visitor,
                           std::function<void(Expression&, const Type*)> check_expression)
        : context(context), sema(sema), resolver(resolver), builder(builder), functions(functions),
          visitor(visitor), check_expression(std::move(check_expression)) {}

    void declare_enum_type(EnumDeclaration& node) {
        if (node.type != nullptr) {
            return;
        }

        resolve_enum_backing(node);
        validate_enum_declaration(node);

        auto* type = builder.build_enum_type(node);
        node.type = type;

        auto* type_scope = sema.env.scopes.create<Scope>(Scope::Kind::Type::Enum, node.name,
                                                         sema.env.current_scope);

        register_type(node.name, node.span, node.visibility, type, node.generic_parameters,
                      node.attributes, type_scope);

        auto* enum_type = type->as<EnumType>();
        if (enum_type != nullptr) {
            evaluate_enum_discriminants(node, *enum_type, type_scope);
        }

        for (auto* method : node.methods) {
            functions.declare_function(*method);
        }

        if (enum_type != nullptr) {
            enum_type->methods.clear();
            enum_type->methods.reserve(node.methods.size());

            std::size_t method_index = 0;
            for (auto* method : node.methods) {
                if (method->function_symbol == nullptr ||
                    method->function_symbol->function_type == nullptr ||
                    method->function_symbol->callable_kind !=
                        FunctionSymbol::Kind::Type::InstanceMethod) {
                    continue;
                }

                enum_type->methods.emplace_back(
                    method->prototype->name, method->function_symbol->function_type, method_index);
                ++method_index;
            }

            validate_interface_conformance(node, *enum_type);
        }
    }
};
