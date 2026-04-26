module;

#include <string>
#include <unordered_map>
#include <vector>

export module zep.frontend.sema.resolver.structure;

import zep.frontend.sema.type;
import zep.frontend.node;
import zep.common.logger.diagnostic;
import zep.frontend.sema.type.resolver;
import zep.common.context;
import zep.frontend.sema.context;
import zep.frontend.sema.resolver.generic;

export class StructureResolver {
  private:
    Visitor<void>& visitor;

    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;

    StructLiteralExpression& node;

    void check_fields(const StructType* struct_type) {
        std::unordered_map<std::string, bool> fields;

        for (auto* literal_field : node.fields) {
            visitor.visit(*literal_field);

            if (fields.contains(literal_field->name)) {
                context.diagnostics.add_error(literal_field->span,
                                              "duplicate field '" + literal_field->name + "'");
                continue;
            }

            fields[literal_field->name] = true;

            bool found = false;

            for (const auto& declared_field : struct_type->fields) {
                if (declared_field.name == literal_field->name) {
                    found = true;

                    const auto* expected_type = resolver.resolve(declared_field.type);
                    const auto* actual_type = literal_field->value->type;
                    if (expected_type == nullptr || actual_type == nullptr) {
                        break;
                    }

                    if (!actual_type->compatible(expected_type)) {
                        context.diagnostics.add_error(literal_field->span,
                                                      "field '" + literal_field->name +
                                                          "' type mismatch: expected '" +
                                                          expected_type->to_string() + "', got '" +
                                                          actual_type->to_string() + "'");
                    }
                    break;
                }
            }

            if (!found) {
                context.diagnostics.add_error(literal_field->span, "struct '" + struct_type->name +
                                                                       "' has no field '" +
                                                                       literal_field->name + "'");
            }
        }

        for (const auto& declared_field : struct_type->fields) {
            if (!fields.contains(declared_field.name)) {
                context.diagnostics.add_error(node.span, "missing field '" + declared_field.name +
                                                             "' in struct '" + struct_type->name +
                                                             "' literal");
            }
        }
    }

  public:
    explicit StructureResolver(Context& context, SemaContext& sema, TypeResolver& resolver,
                               Visitor<void>& visitor, StructLiteralExpression& node)
        : visitor(visitor), context(context), sema(sema), resolver(resolver), node(node) {}

    bool is_valid(const StructType* struct_type) {
        GenericResolver generic_resolver(context, sema, resolver, visitor);
        if (!generic_resolver.check_generic_arguments(
                node.generic_arguments, struct_type->generic_parameters, node.span, true)) {
            return false;
        }

        check_fields(struct_type);

        return true;
    }
};
