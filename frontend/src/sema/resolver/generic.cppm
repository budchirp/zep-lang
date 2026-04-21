module;

#include <string>
#include <vector>

export module zep.frontend.sema.resolver.generic;

import zep.common.span;
import zep.frontend.sema.type;
import zep.frontend.sema.type.type_id;
import zep.frontend.sema.type.type_helper;
import zep.frontend.ast;
import zep.common.logger.diagnostic;
import zep.frontend.sema.type.type_context;
import zep.frontend.sema.context;
export class GenericResolver {
  private:
    Context& context;
    TypeContext& type_context;
    Visitor<void>& visitor;

  public:
    explicit GenericResolver(Context& context, TypeContext& type_context, Visitor<void>& visitor)
        : context(context), type_context(type_context), visitor(visitor) {}

    bool check_generic_arguments(std::vector<GenericArgument*>& generic_arguments,
                                 const std::vector<GenericParameterType>& generic_parameters,
                                 const Span& span, bool emit_errors = false) {
        if (generic_arguments.size() != generic_parameters.size()) {
            if (emit_errors) {
                context.diagnostics.add_error(
                    span, "expected " + std::to_string(generic_parameters.size()) +
                              " generic argument(s), got " +
                              std::to_string(generic_arguments.size()));
            }
            return false;
        }

        bool all_valid = true;

        for (std::size_t i = 0; i < generic_arguments.size(); ++i) {
            if (emit_errors) {
                visitor.visit(*generic_arguments[i]);
            }

            TypeId argument_type = generic_arguments[i]->type->type;
            if (!argument_type.is_valid()) {
                if (!emit_errors) {
                    return false;
                }
                all_valid = false;
                continue;
            }

            const GenericParameterType& generic_parameter = generic_parameters[i];

            if (generic_parameter.constraint.is_valid()) {
                if (!context.type_helper.compatible( argument_type, generic_parameter.constraint)) {
                    if (emit_errors) {
                        context.diagnostics.add_error(
                            generic_arguments[i]->span,
                            "generic argument '" +
                                context.type_helper.type_to_string( argument_type) +
                                "' does not satisfy constraint '" +
                                context.type_helper.type_to_string( generic_parameter.constraint) + "'");
                        all_valid = false;
                    } else {
                        return false;
                    }
                }
            }

            type_context.add_substitution(generic_parameter.name, argument_type);
        }

        return all_valid;
    }

    void define_generic_parameters(const std::vector<GenericParameterType>& generic_parameters) {
        for (const GenericParameterType& generic_parameter : generic_parameters) {
            if (generic_parameter.constraint.is_valid()) {
                type_context.add_substitution(generic_parameter.name, generic_parameter.constraint);
            } else {
                type_context.add_substitution(generic_parameter.name,
                                              context.env.global_scope->lookup_type("any"));
            }
        }
    }
};
