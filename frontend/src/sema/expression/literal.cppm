module;

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

export module zep.frontend.sema.expression.literal;

import zep.common.context;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.constants.evaluator;
import zep.frontend.kind;
import zep.frontend.sema.resolution.facades;
import zep.frontend.sema.types.compatibility;
import zep.frontend.sema.expression.generic;
import zep.frontend.sema.types;
import zep.frontend.sema.types.resolver;

export class LiteralChecker {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    Facades facades;
    Compatibility compatibility;
    Visitor<void>& visitor;
    const Type*& target_type;

    void contextual_arguments(std::vector<GenericArgument*>& arguments,
                              const NominalType& nominal, Span span) {
        if (!arguments.empty() || nominal.generic_parameters.empty() || target_type == nullptr) {
            return;
        }

        const auto* expected = resolver.resolve_type(target_type);
        const auto* expected_nominal = expected != nullptr ? expected->as_nominal() : nullptr;
        if (expected_nominal == nullptr || expected_nominal->definition != nominal.definition ||
            expected_nominal->generic_arguments.size() != nominal.generic_parameters.size()) {
            return;
        }

        for (const auto& argument : expected_nominal->generic_arguments) {
            if (argument.is_const()) {
                return;
            }
        }

        arguments.reserve(nominal.generic_parameters.size());
        for (auto index = std::size_t{0}; index < nominal.generic_parameters.size(); ++index) {
            auto* expression = sema.nodes.create<TypeExpression>(
                span, expected_nominal->generic_arguments[index].type);
            arguments.push_back(sema.nodes.create<GenericArgument>(
                span, nominal.generic_parameters[index].name, expression));
        }
    }

    template <typename Fields>
    std::vector<GenericArgument*>
    infer_from_fields(const std::vector<GenericParameterType>& parameters,
                      const Fields& declared_fields,
                      const std::vector<StructLiteralField*>& literal_fields, Span span,
                      bool emit_errors) {
        GenericInference inference(context, sema, resolver, parameters);

        for (const auto* literal_field : literal_fields) {
            if (literal_field->value == nullptr || literal_field->value->type == nullptr) {
                continue;
            }

            for (const auto& declared : declared_fields) {
                if (declared.name == literal_field->name) {
                    inference.match(declared.type, literal_field->value->type);
                    break;
                }
            }
        }

        return inference.arguments(span, emit_errors);
    }

    template <typename Fields>
    bool apply_field_generic_bindings(std::vector<GenericArgument*>& arguments,
                                      const std::vector<GenericParameterType>& parameters,
                                      const Fields& declared_fields,
                                      const std::vector<StructLiteralField*>& literal_fields,
                                      Span span, bool emit_errors) {
        if (arguments.empty() && !parameters.empty()) {
            arguments = infer_from_fields(parameters, declared_fields, literal_fields, span,
                                          emit_errors);
            if (arguments.empty()) {
                return false;
            }
        }

        GenericArgumentResolver arguments_resolver(context, sema, resolver, visitor);
        return arguments_resolver.apply(arguments, parameters, span, emit_errors);
    }

    template <typename Fields>
    void validate_literal_fields(Span parent_span, const std::string& parent_name,
                                 std::vector<StructLiteralField*>& literal_fields,
                                 const Fields& fields, bool check_visibility) {
        std::unordered_set<std::string> seen;
        seen.reserve(literal_fields.size());

        for (auto* literal_field : literal_fields) {
            if (!seen.insert(literal_field->name).second) {
                context.diagnostics.add_error(literal_field->span,
                                              "duplicate field '" + literal_field->name + "'");
                continue;
            }

            const auto declared = std::ranges::find(fields, literal_field->name, &FieldType::name);
            if (declared == fields.end()) {
                context.diagnostics.add_error(literal_field->span, parent_name + " has no field '" +
                                                                       literal_field->name + "'");
                continue;
            }

            if (check_visibility && (*declared).visibility == Visibility::Type::Private) {
                context.diagnostics.add_error(literal_field->span, "cannot set private field '" +
                                                                       (*declared).name + "' of " +
                                                                       parent_name);
            }

            const auto* expected_type = resolver.resolve_type((*declared).type);
            const auto* actual_type = literal_field->value->type;

            if (expected_type != nullptr && actual_type != nullptr &&
                !compatibility.accepts(expected_type, actual_type)) {
                context.diagnostics.add_error(
                    literal_field->span,
                    "field '" + literal_field->name + "' type mismatch: expected '" +
                        expected_type->to_string() + "', got '" + actual_type->to_string() + "'");
            }
        }

        for (const auto& field : fields) {
            if (!seen.contains(field.name) && !resolver.resolve_type(field.type)->template is<VoidType>()) {
                context.diagnostics.add_error(parent_span, "missing field '" + field.name +
                                                               "' in " + parent_name + " literal");
            }
        }
    }

    template <typename Fields>
    void resolve_field_values(std::vector<StructLiteralField*>& literal_fields,
                              const Fields& declared_fields) {
        for (auto* literal_field : literal_fields) {
            const FieldType* declared = nullptr;

            for (const auto& field : declared_fields) {
                if (field.name == literal_field->name) {
                    declared = &field;
                    break;
                }
            }

            const auto* saved_target_type = target_type;
            if (declared != nullptr) {
                target_type = resolver.resolve_type(declared->type);
            }

            visitor.visit_expression(*literal_field->value);
            if (declared != nullptr) {
                literal_field->value =
                    sema.binding.construct(literal_field->value, declared->type, resolver, context.diagnostics);
            }
            target_type = saved_target_type;
        }
    }

  public:
    LiteralChecker(Context& context, SemaContext& sema, TypeResolver& resolver,
                   Visitor<void>& visitor, const Type*& target_type)
        : context(context), sema(sema), resolver(resolver),
          facades(sema.facades, resolver), compatibility(sema.types, resolver, facades),
          visitor(visitor), target_type(target_type) {}

    void resolve_struct_literal(StructLiteralExpression& node,
                                const NominalType* enclosing_type) {
        visitor.visit_expression(*node.name);

        const auto* struct_type =
            node.name->type != nullptr ? node.name->type->as<StructType>() : nullptr;
        if (struct_type == nullptr) {
            if (node.name->type != nullptr) {
                context.diagnostics.add_error(node.span, "cannot construct non-struct type '" +
                                                             node.name->type->to_string() + "'");
            }
            return;
        }

        contextual_arguments(node.generic_arguments, *struct_type, node.span);
        const auto has_arguments = !node.generic_arguments.empty();

        if (!has_arguments) {
            resolve_field_values(node.fields, struct_type->fields);
        }

        {
            auto scope = resolver.create_substitution_scope();

            if (!apply_field_generic_bindings(node.generic_arguments, struct_type->generic_parameters,
                                              struct_type->fields, node.fields, node.span, true)) {
                return;
            }

            struct_type = resolver.resolve_type(struct_type)->as<StructType>();
        }

        if (has_arguments) {
            resolve_field_values(node.fields, struct_type->fields);
        }

        const auto* definition =
            struct_type->definition != nullptr ? struct_type->definition : struct_type;
        const auto* enclosing_definition =
            enclosing_type != nullptr && enclosing_type->definition != nullptr
                ? enclosing_type->definition
                : enclosing_type;

        validate_literal_fields(node.span, "struct '" + struct_type->name + "'", node.fields,
                                struct_type->fields, definition != enclosing_definition);

        node.type = struct_type;
    }

    bool resolve_variant(QualifiedAccessExpression& node,
                         const std::vector<StructLiteralField*>& fields = {}) {
        const auto* symbol = sema.env.current_scope->lookup_type(node.parent);
        const auto* enumeration = symbol != nullptr ? symbol->type->as<EnumType>() : nullptr;

        if (enumeration == nullptr || enumeration->find_variant(node.member) == nullptr) {
            return false;
        }

        auto* name = sema.nodes.create<IdentifierExpression>(node.span, node.parent);
        EnumVariantExpression literal(node.span, name, node.parent_generic_arguments,
                                      node.member, !fields.empty(), fields);
        resolve_enum_literal(literal);

        node.type = literal.type;
        node.enum_type = literal.enum_type;
        node.variant_type = literal.variant_type;

        if (literal.variant_type != nullptr) {
            node.enum_value = literal.variant_type->discriminant;
        }

        return true;
    }

    void resolve_enum_literal(EnumVariantExpression& node) {
        visitor.visit(*node.enum_name);

        const auto* enum_type =
            node.enum_name->type != nullptr ? node.enum_name->type->as<EnumType>() : nullptr;
        if (enum_type == nullptr) {
            if (node.enum_name->type != nullptr) {
                context.diagnostics.add_error(node.span,
                                              "'" + node.enum_name->name + "' is not an enum type");
            }
            return;
        }

        const auto* raw_variant = enum_type->find_variant(node.variant_name);
        if (raw_variant == nullptr) {
            context.diagnostics.add_error(node.span, "enum '" + enum_type->name +
                                                         "' has no variant '" + node.variant_name +
                                                         "'");
            return;
        }

        contextual_arguments(node.generic_arguments, *enum_type, node.span);
        const auto has_arguments = !node.generic_arguments.empty();

        if (!has_arguments) {
            resolve_field_values(node.fields, enum_type->fields_for(*raw_variant));
        }

        {
            auto scope = resolver.create_substitution_scope();

            if (!apply_field_generic_bindings(node.generic_arguments, enum_type->generic_parameters,
                                              enum_type->fields_for(*raw_variant), node.fields, node.span, true)) {
                return;
            }

            enum_type = resolver.resolve_type(enum_type)->as<EnumType>();
        }

        const auto* variant = enum_type->find_variant(node.variant_name);
        if (variant == nullptr) {
            return;
        }

        if (has_arguments) {
            resolve_field_values(node.fields, enum_type->fields_for(*variant));
        }

        const auto label = enum_type->name + "::" + variant->name;
        const auto fields = enum_type->fields_for(*variant);
        const auto requires_payload = std::ranges::any_of(fields, [&](const FieldType& field) -> bool {
            return !resolver.resolve_type(field.type)->is<VoidType>();
        });

        if (!node.has_payload && requires_payload) {
            context.diagnostics.add_error(node.span,
                                          "enum variant '" + label + "' requires payload");
            return;
        }

        if (node.has_payload && fields.empty()) {
            context.diagnostics.add_error(node.span, "enum variant '" + label + "' has no payload");
            return;
        }

        validate_literal_fields(node.span, "enum variant '" + label + "'", node.fields, fields,
                                false);

        node.enum_type = enum_type;
        node.variant_type = variant;
        node.type = enum_type;
    }
};
