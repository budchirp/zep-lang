module;

#include <string>
#include <unordered_set>
#include <vector>

export module zep.frontend.sema.resolver.when;

import zep.common.context;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.env;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.type.resolver;

export class WhenResolver {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& type_resolver;
    Visitor<void>& visitor;

    static bool is_comparable(const Type* type) {
        return type != nullptr &&
               (type->is_numeric() || type->is<BooleanType>() || type->is<PointerType>());
    }

    void validate_boolean_condition(Expression& node, const std::string& description) {
        if (node.type != nullptr && !node.type->is<BooleanType>()) {
            context.diagnostics.add_error(node.span, description + " must be boolean, got '" +
                                                         node.type->to_string() + "'");
        }
    }

    void validate_expression_pattern(WhenPattern& pattern, const Type* subject_type,
                                     bool has_subject, bool subject_comparable) {
        if (pattern.expression == nullptr) {
            return;
        }

        auto& expression = *pattern.expression;

        if (!has_subject) {
            visitor.visit_expression(expression);
            validate_boolean_condition(expression, "when arm condition");
            return;
        }

        if (subject_type != nullptr && subject_type->is<EnumType>()) {
            context.diagnostics.add_error(pattern.span,
                                          "enum when subject requires enum variant patterns");
            visitor.visit_expression(expression);
            return;
        }

        visitor.visit_expression(expression);

        const auto* pattern_type = expression.type;
        if (subject_type == nullptr || pattern_type == nullptr) {
            return;
        }

        if (!subject_type->accepts(pattern_type)) {
            context.diagnostics.add_error(expression.span,
                                          "when arm condition type mismatch: expected '" +
                                              subject_type->to_string() + "', got '" +
                                              pattern_type->to_string() + "'");
        }

        if (subject_comparable && !is_comparable(pattern_type)) {
            context.diagnostics.add_error(expression.span,
                                          "when arm condition type '" + pattern_type->to_string() +
                                              "' is not supported in v1 when expressions");
        }
    }

    void validate_enum_pattern(WhenPattern& pattern, const Type* subject_type, bool has_subject,
                               bool multiple_patterns) {
        if (!has_subject) {
            context.diagnostics.add_error(pattern.span,
                                          "enum variant pattern requires a when subject");
            return;
        }

        const auto* subject_enum = subject_type != nullptr ? subject_type->as<EnumType>() : nullptr;
        if (subject_enum == nullptr) {
            context.diagnostics.add_error(pattern.span,
                                          "enum variant pattern requires an enum subject");
            return;
        }

        if (pattern.enum_name == nullptr) {
            return;
        }

        visitor.visit(*pattern.enum_name);

        const auto* pattern_enum =
            pattern.enum_name->type != nullptr ? pattern.enum_name->type->as<EnumType>() : nullptr;

        if (pattern_enum == nullptr) {
            context.diagnostics.add_error(pattern.span,
                                          "'" + pattern.enum_name->name + "' is not an enum type");
            return;
        }

        if (pattern_enum->name != subject_enum->name) {
            context.diagnostics.add_error(
                pattern.span, "when enum pattern type mismatch: expected '" + subject_enum->name +
                                  "', got '" + pattern_enum->name + "'");
            return;
        }

        const auto* variant = subject_enum->find_variant(pattern.variant_name);
        if (variant == nullptr) {
            context.diagnostics.add_error(pattern.span, "enum '" + subject_enum->name +
                                                            "' has no variant '" +
                                                            pattern.variant_name + "'");
            return;
        }

        if (subject_enum->backing_type != nullptr &&
            !subject_enum->backing_type->is<IntegerType>() &&
            !subject_enum->backing_type->is<BooleanType>()) {
            context.diagnostics.add_error(
                pattern.span,
                "backed enum '" + subject_enum->name +
                    "' cannot be used in when patterns unless its backing type is scalar");
            return;
        }

        pattern.enum_type = subject_enum;
        pattern.variant_type = variant;

        apply_pattern_bindings(pattern, *subject_enum, *variant, multiple_patterns);
    }

    void validate_pattern(WhenPattern& pattern, const Type* subject_type, bool has_subject,
                          bool subject_comparable, bool multiple_patterns) {
        if (pattern.pattern_kind == WhenPattern::PatternKind::Type::Expression) {
            validate_expression_pattern(pattern, subject_type, has_subject, subject_comparable);
        } else {
            validate_enum_pattern(pattern, subject_type, has_subject, multiple_patterns);
        }
    }

    void apply_pattern_bindings(WhenPattern& pattern, const EnumType& subject_enum,
                                const EnumVariantType& variant, bool multiple_patterns) {
        const auto label = subject_enum.name + "::" + variant.name;

        if (variant.fields.empty()) {
            if (!pattern.fields.empty()) {
                context.diagnostics.add_error(pattern.span,
                                              "enum variant '" + label + "' has no payload");
            }
            return;
        }

        if (pattern.fields.empty()) {
            context.diagnostics.add_error(pattern.span,
                                          "enum variant '" + label + "' requires payload pattern");
            return;
        }

        if (multiple_patterns) {
            context.diagnostics.add_error(
                pattern.span, "enum payload binding patterns cannot be combined in one when arm");
            return;
        }

        std::unordered_set<std::string> seen_fields;
        std::unordered_set<std::string> seen_bindings;

        seen_fields.reserve(pattern.fields.size());
        seen_bindings.reserve(pattern.fields.size());

        for (auto* field : pattern.fields) {
            if (!seen_fields.insert(field->field_name).second) {
                context.diagnostics.add_error(field->span, "duplicate pattern field '" +
                                                               field->field_name + "'");
                continue;
            }

            if (!seen_bindings.insert(field->binding_name).second) {
                context.diagnostics.add_error(field->span, "duplicate pattern binding '" +
                                                               field->binding_name + "'");
                continue;
            }

            const auto* declared_field = variant.find_field(field->field_name);

            if (declared_field == nullptr) {
                context.diagnostics.add_error(field->span, "enum variant '" + label +
                                                               "' has no field '" +
                                                               field->field_name + "'");
                continue;
            }

            const auto* field_type = type_resolver.resolve_type(declared_field->type);
            sema.env.current_scope->define_var(
                field->binding_name,
                sema.env.symbols.create<VariableSymbol>(field->binding_name, field->span,
                                                        Visibility::Type::Private,
                                                        StorageKind::Type::Var, field_type));
        }

        for (const auto& field : variant.fields) {
            if (!seen_fields.contains(field.name)) {
                context.diagnostics.add_error(pattern.span, "missing field '" + field.name +
                                                                "' in enum pattern '" + label +
                                                                "'");
            }
        }
    }

  public:
    explicit WhenResolver(Context& context, SemaContext& sema, TypeResolver& type_resolver,
                          Visitor<void>& visitor)
        : context(context), sema(sema), type_resolver(type_resolver), visitor(visitor) {}

    void resolve(WhenExpression& node) {
        const Type* subject_type = nullptr;
        auto subject_comparable = true;

        if (node.subject != nullptr) {
            visitor.visit_expression(*node.subject);
            subject_type = node.subject->type;

            if (subject_type != nullptr && !subject_type->is<EnumType>()) {
                subject_comparable = is_comparable(subject_type);

                if (!subject_comparable) {
                    context.diagnostics.add_error(
                        node.subject->span, "when subject type '" + subject_type->to_string() +
                                                "' is not supported in v1 when expressions");
                }
            }
        }

        auto has_else = false;
        const Type* result_type = nullptr;

        std::unordered_set<std::string> seen_variants;
        for (std::size_t i = 0; i < node.arms.size(); ++i) {
            auto* arm = node.arms[i];

            if (arm->is_else) {
                if (has_else) {
                    context.diagnostics.add_error(arm->span,
                                                  "duplicate else arm in when expression");
                }

                has_else = true;

                if (i + 1 < node.arms.size()) {
                    context.diagnostics.add_error(arm->span,
                                                  "else arm must be last in when expression");
                }
            } else {
                if (has_else) {
                    context.diagnostics.add_error(arm->span,
                                                  "else arm must be last in when expression");
                }

                if (arm->patterns.empty()) {
                    context.diagnostics.add_error(arm->span,
                                                  "when arm must have at least one condition");
                }
            }

            {
                ScopeGuard arm_scope(sema.env.current_scope, sema.env.scopes,
                                     Scope::Kind::Type::Block, "when_arm");

                if (!arm->is_else) {
                    const auto multiple_patterns = arm->patterns.size() > 1;

                    for (auto* pattern : arm->patterns) {
                        validate_pattern(*pattern, subject_type, node.subject != nullptr,
                                         subject_comparable, multiple_patterns);

                        if (arm->guard == nullptr &&
                            pattern->pattern_kind == WhenPattern::PatternKind::Type::EnumVariant) {
                            if (pattern->variant_type != nullptr) {
                                seen_variants.insert(pattern->variant_type->name);
                            }
                        }
                    }

                    if (arm->guard != nullptr) {
                        visitor.visit_expression(*arm->guard);
                        validate_boolean_condition(*arm->guard, "when arm guard");
                    }
                }

                visitor.visit_statement(*arm->body);
            }

            const auto* body_type = arm->body->type;
            if (body_type == nullptr) {
                continue;
            }

            if (result_type == nullptr) {
                result_type = body_type;
                continue;
            }

            if (!result_type->accepts(body_type)) {
                context.diagnostics.add_error(arm->body->span,
                                              "when arm type mismatch: expected '" +
                                                  result_type->to_string() + "', got '" +
                                                  body_type->to_string() + "'");
            }
        }

        auto exhaustive = false;
        if (subject_type != nullptr && subject_type->is<EnumType>()) {
            const auto* subject_enum = subject_type->as<EnumType>();
            exhaustive = seen_variants.size() == subject_enum->variants.size();
        }

        node.is_exhaustive = exhaustive;

        if (!has_else && !exhaustive) {
            context.diagnostics.add_error(node.span, "when expression requires an else arm");
        }

        node.type =
            result_type != nullptr ? result_type : sema.builtin_resolver.primitives.at("void");
    }
};
