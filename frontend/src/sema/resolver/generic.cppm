module;

#include <string>
#include <vector>

export module zep.frontend.sema.resolver.generic;

import zep.common.span;
import zep.frontend.sema.type;
import zep.frontend.ast;
import zep.common.logger.diagnostic;
import zep.frontend.sema.type.type_context;
import zep.frontend.sema.context;
import zep.frontend.sema.env;
import zep.frontend.sema.scope;

export class GenericResolver {
  private:
    Visitor<void>& visitor;

    Context& context;
    TypeContext& type_context;

  public:
    explicit GenericResolver(Context& context, TypeContext& type_context, Visitor<void>& visitor)
        : visitor(visitor), context(context), type_context(type_context) {}

    bool check_generic_arguments(std::vector<GenericArgument*>& generic_arguments,
                                 const std::vector<GenericParameterType>& generic_parameters,
                                 const Span& span, bool emit_errors = false) {
        if (generic_arguments.size() != generic_parameters.size()) {
            if (emit_errors) {
                context.diagnostics.add_error(span, "expected " +
                                                        std::to_string(generic_parameters.size()) +
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

            const auto* argument_type = generic_arguments[i]->type->type;
            if (argument_type == nullptr) {
                if (!emit_errors) {
                    return false;
                }

                all_valid = false;
                continue;
            }

            const auto& generic_parameter = generic_parameters[i];

            if (generic_parameter.constraint != nullptr) {
                if (!argument_type->compatible(generic_parameter.constraint)) {
                    if (emit_errors) {
                        context.diagnostics.add_error(
                            generic_arguments[i]->span,
                            "generic argument '" + argument_type->to_string() +
                                "' does not satisfy constraint '" +
                                generic_parameter.constraint->to_string() + "'");

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
        for (const auto& generic_parameter : generic_parameters) {
            if (generic_parameter.constraint != nullptr) {
                type_context.add_substitution(generic_parameter.name, generic_parameter.constraint);
            } else {
                type_context.add_substitution(generic_parameter.name,
                                              context.env.primitives["any"]);
            }
        }
    }
};
