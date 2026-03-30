module;

#include <memory>
#include <string>
#include <vector>

export module zep.frontend.sema.checker.generic_resolver;

import zep.common.position;
import zep.frontend.sema.type;
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

    bool check_generic_arguments(
        std::vector<std::unique_ptr<GenericArgument>>& generic_arguments,
        const std::vector<std::shared_ptr<GenericParameterType>>& generic_parameters,
        const Position& position, bool emit_errors = false) {
        if (generic_arguments.size() != generic_parameters.size()) {
            if (emit_errors) {
                context.diagnostics.add_error(
                    position, "expected " + std::to_string(generic_parameters.size()) +
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

            auto argument_type = generic_arguments[i]->type->get_type();
            if (argument_type == nullptr) {
                if (!emit_errors) {
                    return false;
                }
                all_valid = false;
                continue;
            }

            const auto& generic_parameter = generic_parameters[i];

            if (generic_parameter->constraint != nullptr) {
                if (!Type::compatible(argument_type, generic_parameter->constraint)) {
                    if (emit_errors) {
                        context.diagnostics.add_error(
                            generic_arguments[i]->position,
                            "generic argument '" + argument_type->to_string() +
                                "' does not satisfy constraint '" +
                                generic_parameter->constraint->to_string() + "'");
                        all_valid = false;
                    } else {
                        return false;
                    }
                }
            }

            type_context.add_substitution(generic_parameter->name, argument_type);
        }

        return all_valid;
    }

    void define_generic_parameters(
        const std::vector<std::shared_ptr<GenericParameterType>>& generic_parameters) {
        for (const auto& generic_parameter : generic_parameters) {
            if (generic_parameter->constraint) {
                type_context.add_substitution(generic_parameter->name,
                                              generic_parameter->constraint);
            } else {
                type_context.add_substitution(generic_parameter->name, std::make_shared<AnyType>());
            }
        }
    }
};
