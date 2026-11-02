module;

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
import zep.frontend.sema.constant.evaluator;
import zep.frontend.kind;
import zep.frontend.sema.lifetime;
import zep.frontend.sema.facade;
import zep.frontend.sema.expression.generic;
import zep.frontend.sema.type;
import zep.frontend.sema.type_resolver;

export class LiteralChecker {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    FacadeResolver facades;
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

    std::vector<GenericArgument*>
    infer_from_fields(const std::vector<GenericParameterType>& parameters,
                      const std::vector<FieldType>& declared_fields,
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

    bool apply_field_generic_bindings(std::vector<GenericArgument*>& arguments,
                                      const std::vector<GenericParameterType>& parameters,
                                      const std::vector<FieldType>& declared_fields,
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

    template <typename FieldOwner>
    void validate_literal_fields(Span parent_span, const std::string& parent_name,
                                 std::vector<StructLiteralField*>& literal_fields,
                                 const FieldOwner& owner, bool check_visibility) {
        std::unordered_set<std::string> seen;
        seen.reserve(literal_fields.size());

        for (auto* literal_field : literal_fields) {
            if (!seen.insert(literal_field->name).second) {
                context.diagnostics.add_error(literal_field->span,
                                              "duplicate field '" + literal_field->name + "'");
                continue;
            }

            const auto* declared = owner.find_field(literal_field->name);
            if (declared == nullptr) {
                context.diagnostics.add_error(literal_field->span, parent_name + " has no field '" +
                                                                       literal_field->name + "'");
                continue;
            }

            if (check_visibility && declared->visibility == Visibility::Type::Private) {
                context.diagnostics.add_error(literal_field->span, "cannot set private field '" +
                                                                       declared->name + "' of " +
                                                                       parent_name);
            }

            const auto* expected_type = resolver.resolve_type(declared->type);
            const auto* actual_type = literal_field->value->type;

            if (expected_type != nullptr && actual_type != nullptr &&
                !facades.accepts(expected_type, actual_type)) {
                context.diagnostics.add_error(
                    literal_field->span,
                    "field '" + literal_field->name + "' type mismatch: expected '" +
                        expected_type->to_string() + "', got '" + actual_type->to_string() + "'");
            }
        }

        for (const auto& field : owner.fields) {
            if (!seen.contains(field.name)) {
                context.diagnostics.add_error(parent_span, "missing field '" + field.name +
                                                               "' in " + parent_name + " literal");
            }
        }
    }

    void resolve_field_values(std::vector<StructLiteralField*>& literal_fields,
                              const std::vector<FieldType>& declared_fields) {
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
                    sema.lifetime.copy_construct(literal_field->value, declared->type, resolver);
            }
            target_type = saved_target_type;
        }
    }

  public:
    LiteralChecker(Context& context, SemaContext& sema, TypeResolver& resolver,
                   Visitor<void>& visitor, const Type*& target_type)
        : context(context), sema(sema), resolver(resolver),
          facades(sema.facades, sema.types, resolver),
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

        auto scope = resolver.create_substitution_scope();

        contextual_arguments(node.generic_arguments, *struct_type, node.span);
        const auto has_arguments = !node.generic_arguments.empty();
        if (has_arguments) {
            GenericArgumentResolver arguments(context, sema, resolver, visitor);
            if (!arguments.apply(node.generic_arguments, struct_type->generic_parameters,
                                 node.span, true)) {
                return;
            }
        }

        resolve_field_values(node.fields, struct_type->fields);

        if (!has_arguments) {
            if (!apply_field_generic_bindings(node.generic_arguments, struct_type->generic_parameters,
                                              struct_type->fields, node.fields, node.span, true)) {
                return;
            }
        }

        const auto* definition =
            struct_type->definition != nullptr ? struct_type->definition : struct_type;
        const auto* enclosing_definition =
            enclosing_type != nullptr && enclosing_type->definition != nullptr
                ? enclosing_type->definition
                : enclosing_type;

        validate_literal_fields(node.span, "struct '" + struct_type->name + "'", node.fields,
                                *struct_type, definition != enclosing_definition);

        node.type = resolver.resolve_type(struct_type);
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

        auto scope = resolver.create_substitution_scope();

        contextual_arguments(node.generic_arguments, *enum_type, node.span);
        const auto has_arguments = !node.generic_arguments.empty();
        if (has_arguments) {
            GenericArgumentResolver arguments(context, sema, resolver, visitor);
            if (!arguments.apply(node.generic_arguments, enum_type->generic_parameters,
                                 node.span, true)) {
                return;
            }
        }

        resolve_field_values(node.fields, raw_variant->fields);

        if (!has_arguments) {
            if (!apply_field_generic_bindings(node.generic_arguments, enum_type->generic_parameters,
                                              raw_variant->fields, node.fields, node.span, true)) {
                return;
            }
        }

        const auto* resolved_enum = resolver.resolve_type(enum_type);
        if (resolved_enum != nullptr && resolved_enum->is<EnumType>()) {
            enum_type = resolved_enum->as<EnumType>();
        }

        const auto* variant = enum_type->find_variant(node.variant_name);
        if (variant == nullptr) {
            return;
        }

        const auto label = enum_type->name + "::" + variant->name;
        if (!node.has_payload && !variant->fields.empty()) {
            context.diagnostics.add_error(node.span,
                                          "enum variant '" + label + "' requires payload");
            return;
        }

        if (node.has_payload && variant->fields.empty()) {
            context.diagnostics.add_error(node.span, "enum variant '" + label + "' has no payload");
            return;
        }

        validate_literal_fields(node.span, "enum variant '" + label + "'", node.fields, *variant,
                                false);

        node.enum_type = enum_type;
        node.variant_type = variant;
        node.type = enum_type;
    }
};
