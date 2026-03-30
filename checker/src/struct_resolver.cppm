module;

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

export module zep.checker.struct_resolver;

import zep.common.position;
import zep.frontend.sema.type;
import zep.frontend.ast;
import zep.common.logger.diagnostic;
import zep.frontend.sema.type.type_context;
import zep.frontend.sema.context;
import zep.checker.generic_resolver;

export class StructResolver {
  private:
    Context& context;
    TypeContext& type_context;
    Visitor<void>& visitor;

    void check_fields(const StructType* struct_type, StructLiteralExpression& node) {
        std::unordered_map<std::string, bool> provided_fields;

        for (auto& literal_field : node.fields) {
            visitor.visit(*literal_field);

            if (provided_fields.contains(literal_field->name)) {
                context.diagnostics.add_error(literal_field->position,
                                              "duplicate field '" + literal_field->name + "'");
                continue;
            }

            provided_fields[literal_field->name] = true;

            bool found = false;

            for (const auto& declared_field : struct_type->fields) {
                if (declared_field->name == literal_field->name) {
                    found = true;

                    auto expected_type = type_context.resolve_type(declared_field->type);
                    auto actual_type = literal_field->value->get_type();

                    if (!Type::compatible(actual_type, expected_type)) {
                        context.diagnostics.add_error(literal_field->position,
                                                      "field '" + literal_field->name +
                                                          "' type mismatch: expected '" +
                                                          expected_type->to_string() + "', got '" +
                                                          actual_type->to_string() + "'");
                    }
                    break;
                }
            }

            if (!found) {
                context.diagnostics.add_error(literal_field->position,
                                              "struct '" + struct_type->name + "' has no field '" +
                                                  literal_field->name + "'");
            }
        }

        for (const auto& declared_field : struct_type->fields) {
            if (!provided_fields.contains(declared_field->name)) {
                context.diagnostics.add_error(
                    node.position, "missing field '" + declared_field->name + "' in struct '" +
                                       struct_type->name + "' literal");
            }
        }
    }

  public:
    explicit StructResolver(Context& context, TypeContext& type_context, Visitor<void>& visitor)
        : context(context), type_context(type_context), visitor(visitor) {}

    void is_valid(const StructType* struct_type, StructLiteralExpression& node) {
        GenericResolver generic_resolver(context, type_context, visitor);
        generic_resolver.check_generic_arguments(
            node.generic_arguments, struct_type->generic_parameters, node.position, true);

        check_fields(struct_type, node);
    }
};
