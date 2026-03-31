module;

#include <memory>
#include <string>
#include <vector>

export module zep.frontend.sema.checker.call_resolver;

import zep.common.position;
import zep.frontend.sema.type;
import zep.frontend.ast;
import zep.common.logger.diagnostic;
import zep.frontend.sema.type.type_context;
import zep.frontend.sema.context;
import zep.frontend.sema.checker.generic_resolver;
import zep.frontend.sema.symbol;

export class CallResolver {
  private:
    Context& context;
    TypeContext& type_context;
    Visitor<void>& visitor;

    bool check_arguments(const FunctionType* candidate_type, CallExpression& node,
                         bool emit_errors = false) {
        auto parameter_count = candidate_type->parameters.size();
        auto required =
            candidate_type->variadic && parameter_count > 0 ? parameter_count - 1 : parameter_count;

        if (candidate_type->variadic) {
            if (node.arguments.size() < required) {
                if (emit_errors) {
                    context.diagnostics.add_error(node.position,
                                                  "expected at least " + std::to_string(required) +
                                                      " argument(s), got " +
                                                      std::to_string(node.arguments.size()));
                }
                return false;
            }
        } else if (node.arguments.size() != parameter_count) {
            if (emit_errors) {
                context.diagnostics.add_error(node.position,
                                              "expected " + std::to_string(parameter_count) +
                                                  " argument(s), got " +
                                                  std::to_string(node.arguments.size()));
            }
            return false;
        }

        bool all_valid = true;

        for (std::size_t i = 0; i < required; ++i) {
            auto expected_type = type_context.resolve_type(candidate_type->parameters[i]->type);
            auto actual_type = node.arguments[i]->value->get_type();

            if (actual_type == nullptr || !Type::compatible(actual_type, expected_type)) {
                if (emit_errors) {
                    context.diagnostics.add_error(
                        node.arguments[i]->position,
                        "argument type mismatch: expected '" + expected_type->to_string() +
                            "', got '" + (actual_type ? actual_type->to_string() : "unknown") +
                            "'");
                    all_valid = false;
                } else {
                    return false;
                }
            }
        }

        if (candidate_type->variadic && parameter_count > 0) {
            auto element_type = type_context.resolve_type(candidate_type->parameters.back()->type);
            if (element_type == nullptr) {
                return false;
            } else {
                if (auto* array_type = element_type->as<ArrayType>()) {
                    element_type = array_type->element;
                } else if (emit_errors) {
                    context.diagnostics.add_error(node.position, "expected array type, got '" +
                                                                     element_type->to_string() +
                                                                     "'");
                    return false;
                } else {
                    return false;
                }

                for (std::size_t i = required; i < node.arguments.size(); ++i) {
                    auto actual_type = node.arguments[i]->value->get_type();
                    if (actual_type == nullptr || !Type::compatible(actual_type, element_type)) {
                        if (emit_errors) {
                            context.diagnostics.add_error(
                                node.arguments[i]->position,
                                "argument type mismatch: expected '" + element_type->to_string() +
                                    "', got '" +
                                    (actual_type ? actual_type->to_string() : "unknown") + "'");
                            all_valid = false;
                        } else {
                            return false;
                        }
                    }
                }
            }
        }

        return all_valid;
    }

  public:
    explicit CallResolver(Context& context, TypeContext& type_context, Visitor<void>& visitor)
        : context(context), type_context(type_context), visitor(visitor) {}

    const FunctionType* resolve_overload(const std::string& name, CallExpression& node) {
        for (auto& generic_argument : node.generic_arguments) {
            visitor.visit(*generic_argument);
        }

        int match_count = 0;
        const auto* best_match = context.env.current_scope->resolve_overload(
            name,
            [this, &node](const FunctionType* candidate_type) {
                auto scope = type_context.scoped_substitutions();
                return is_valid(candidate_type, node, false);
            },
            match_count);

        if (match_count > 1) {
            context.diagnostics.add_error(node.position, "ambiguous call to '" + name + "'");
            return nullptr;
        }

        if (match_count == 1) {
            return best_match->type->as<FunctionType>();
        }

        return nullptr;
    }

    bool is_valid(const FunctionType* candidate_type, CallExpression& node,
                  bool emit_errors = false) {
        GenericResolver generic_resolver(context, type_context, visitor);
        if (!generic_resolver.check_generic_arguments(node.generic_arguments,
                                                      candidate_type->generic_parameters,
                                                      node.position, emit_errors)) {
            return false;
        }

        if (!check_arguments(candidate_type, node, emit_errors)) {
            return false;
        }

        return true;
    }
};
