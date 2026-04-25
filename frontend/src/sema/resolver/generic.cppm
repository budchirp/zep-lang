module;

#include <string>
#include <vector>

export module zep.frontend.sema.resolver.generic;

import zep.common.span;
import zep.frontend.sema.type;
import zep.frontend.ast;
import zep.common.logger.diagnostic;
import zep.frontend.sema.type.resolver;
import zep.frontend.sema.context;
import zep.frontend.sema.env;
import zep.frontend.sema.scope;

export class GenericResolver {
  private:
    Visitor<void>& visitor;

    Context& context;
    TypeResolver& resolver;

  public:
    explicit GenericResolver(Context& context, TypeResolver& resolver, Visitor<void>& visitor)
        : visitor(visitor), context(context), resolver(resolver) {}

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
            visitor.visit(*generic_arguments[i]);

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

            resolver.add_substitution(generic_parameter.name, argument_type);
        }

        return all_valid;
    }

    void
    activate_generic_parameters(const std::vector<GenericParameterType>& generic_parameters,
                                bool as_self = false) {
        for (const auto& generic_parameter : generic_parameters) {
            if (as_self) {
                resolver.add_substitution(
                    generic_parameter.name,
                    context.types.create<NamedType>(generic_parameter.name,
                                                    std::vector<GenericArgumentType>{}));
            } else {
                resolver.add_substitution(generic_parameter.name,
                                          generic_parameter.constraint != nullptr
                                              ? generic_parameter.constraint
                                              : context.env.primitives["any"]);
            }
        }
    }
};
