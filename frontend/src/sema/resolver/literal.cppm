module;

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

export module zep.frontend.sema.resolver.literal;

import zep.common.context;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.constant.evaluator;
import zep.frontend.sema.kind;
import zep.frontend.sema.resolver.facade;
import zep.frontend.sema.resolver.generic;
import zep.frontend.sema.type;
import zep.frontend.sema.type.resolver;

export class LiteralResolver {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    FacadeResolver facades;
    Visitor<void>& visitor;
    const Type*& target_type;

    static void infer_from_type_match(const Type* declared, const Type* actual,
                                      std::unordered_map<std::string, const Type*>& inferred) {
        if (declared == nullptr || actual == nullptr) {
            return;
        }

        if (const auto* named = declared->as<NamedType>(); named != nullptr) {
            if (named->generic_arguments.empty()) {
                inferred.insert_or_assign(named->name, actual);
            }
            return;
        }

        if (const auto* declared_pointer = declared->as<PointerType>();
            declared_pointer != nullptr) {
            if (const auto* actual_pointer = actual->as<PointerType>(); actual_pointer != nullptr) {
                infer_from_type_match(declared_pointer->element, actual_pointer->element, inferred);
            }
            return;
        }

        if (const auto* declared_array = declared->as<ArrayType>(); declared_array != nullptr) {
            if (const auto* actual_array = actual->as<ArrayType>(); actual_array != nullptr) {
                infer_from_type_match(declared_array->element, actual_array->element, inferred);
            }
        }
    }

    std::vector<GenericArgument*>
    infer_from_fields(const std::vector<GenericParameterType>& parameters,
                      const std::vector<FieldType>& declared_fields,
                      const std::vector<StructLiteralField*>& literal_fields, Span span) {
        std::unordered_map<std::string, const Type*> inferred;
        inferred.reserve(parameters.size());

        for (const auto* literal_field : literal_fields) {
            if (literal_field->value == nullptr || literal_field->value->type == nullptr) {
                continue;
            }

            for (const auto& declared : declared_fields) {
                if (declared.name == literal_field->name) {
                    infer_from_type_match(declared.type, literal_field->value->type, inferred);
                    break;
                }
            }
        }

        if (inferred.size() != parameters.size()) {
            return {};
        }

        std::vector<GenericArgument*> arguments;
        arguments.reserve(parameters.size());

        for (const auto& parameter : parameters) {
            auto iterator = inferred.find(parameter.name);
            if (iterator == inferred.end() || iterator->second == nullptr) {
                return {};
            }

            auto* type_expression = sema.nodes.create<TypeExpression>(span, iterator->second);
            arguments.push_back(
                sema.nodes.create<GenericArgument>(span, parameter.name, type_expression));
        }

        return arguments;
    }

    bool reorder_arguments(std::vector<GenericArgument*>& arguments,
                           const std::vector<GenericParameterType>& parameters, Span span,
                           bool emit_errors) {
        if (arguments.size() != parameters.size()) {
            if (emit_errors) {
                context.diagnostics.add_error(
                    span, "expected " + std::to_string(parameters.size()) +
                              " generic argument(s), got " + std::to_string(arguments.size()));
            }

            return false;
        }

        std::vector<GenericArgument*> ordered(parameters.size(), nullptr);
        std::unordered_set<std::string> used_names;
        used_names.reserve(arguments.size());

        bool seen_named = false;
        bool valid = true;
        std::size_t positional_index = 0;

        for (auto* argument : arguments) {
            if (argument->name.empty()) {
                if (seen_named) {
                    if (emit_errors) {
                        context.diagnostics.add_error(
                            argument->span,
                            "positional generic argument cannot follow named generic argument");
                    }

                    valid = false;
                    continue;
                }

                ordered[positional_index] = argument;
                used_names.insert(parameters[positional_index].name);
                ++positional_index;
                continue;
            }

            seen_named = true;

            std::size_t parameter_index = 0;
            for (; parameter_index < parameters.size(); ++parameter_index) {
                if (parameters[parameter_index].name == argument->name) {
                    break;
                }
            }

            if (parameter_index == parameters.size()) {
                if (emit_errors) {
                    context.diagnostics.add_error(argument->span, "unknown generic argument '" +
                                                                      argument->name + "'");
                }

                valid = false;
                continue;
            }

            if (!used_names.insert(argument->name).second) {
                if (emit_errors) {
                    context.diagnostics.add_error(argument->span, "duplicate generic argument '" +
                                                                      argument->name + "'");
                }

                valid = false;
                continue;
            }

            ordered[parameter_index] = argument;
        }

        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (ordered[i] != nullptr) {
                continue;
            }

            if (emit_errors) {
                context.diagnostics.add_error(span, "missing generic argument '" +
                                                        parameters[i].name + "'");
            }

            valid = false;
        }

        if (!valid) {
            return false;
        }

        arguments = std::move(ordered);
        return true;
    }

    bool apply_generic_bindings(std::vector<GenericArgument*>& arguments,
                                const std::vector<GenericParameterType>& parameters, Span span,
                                bool emit_errors) {
        if (!reorder_arguments(arguments, parameters, span, emit_errors)) {
            return false;
        }

        GenericArgumentResolver arguments_resolver(context, sema, resolver, visitor);
        return arguments_resolver.apply(arguments, parameters, emit_errors);
    }

    bool apply_field_generic_bindings(std::vector<GenericArgument*>& arguments,
                                      const std::vector<GenericParameterType>& parameters,
                                      const std::vector<FieldType>& declared_fields,
                                      const std::vector<StructLiteralField*>& literal_fields,
                                      Span span, bool emit_errors) {
        if (arguments.empty() && !parameters.empty()) {
            auto inferred = infer_from_fields(parameters, declared_fields, literal_fields, span);
            if (!inferred.empty()) {
                arguments = std::move(inferred);
            }
        }

        return apply_generic_bindings(arguments, parameters, span, emit_errors);
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
                target_type = declared->type;
            }

            visitor.visit_expression(*literal_field->value);
            target_type = saved_target_type;
        }
    }

  public:
    LiteralResolver(Context& context, SemaContext& sema, TypeResolver& resolver,
                    Visitor<void>& visitor, const Type*& target_type)
        : context(context), sema(sema), resolver(resolver), facades(sema, resolver),
          visitor(visitor), target_type(target_type) {}

    void resolve_struct_literal(StructLiteralExpression& node, const std::string& current_parent) {
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

        resolve_field_values(node.fields, struct_type->fields);

        auto scope = resolver.create_substitution_scope();

        if (!apply_field_generic_bindings(node.generic_arguments, struct_type->generic_parameters,
                                          struct_type->fields, node.fields, node.span, true)) {
            return;
        }

        validate_literal_fields(node.span, "struct '" + struct_type->name + "'", node.fields,
                                *struct_type, struct_type->name != current_parent);

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

        resolve_field_values(node.fields, raw_variant->fields);

        auto scope = resolver.create_substitution_scope();

        if (!apply_field_generic_bindings(node.generic_arguments, enum_type->generic_parameters,
                                          raw_variant->fields, node.fields, node.span, true)) {
            return;
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
