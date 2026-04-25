module;

#include <string>
#include <vector>

export module zep.frontend.sema.resolver.call;

import zep.frontend.sema.type;
import zep.frontend.ast;
import zep.common.logger.diagnostic;
import zep.frontend.sema.type.type_context;
import zep.frontend.sema.context;
import zep.frontend.sema.env;
import zep.frontend.sema.scope;
import zep.frontend.sema.resolver.generic;
import zep.frontend.sema.symbol;

export class CallResolver {
  private:
    Visitor<void>& visitor;

    Context& context;
    TypeContext& type_context;

    bool check_arguments(const FunctionType* function_type, CallExpression& node,
                         bool emit_errors = false) {
        std::size_t parameter_count = function_type->parameters.size();
        std::size_t required =
            function_type->variadic && parameter_count > 0 ? parameter_count - 1 : parameter_count;

        if (function_type->variadic) {
            if (node.arguments.size() < required) {
                if (emit_errors) {
                    context.diagnostics.add_error(node.span,
                                                  "expected at least " + std::to_string(required) +
                                                      " argument(s), got " +
                                                      std::to_string(node.arguments.size()));
                }

                return false;
            }
        } else if (node.arguments.size() != parameter_count) {
            if (emit_errors) {
                context.diagnostics.add_error(
                    node.span, "expected " + std::to_string(parameter_count) +
                                   " argument(s), got " + std::to_string(node.arguments.size()));
            }

            return false;
        }

        bool all_valid = true;

        for (std::size_t i = 0; i < required; ++i) {
            const auto* expected_type =
                type_context.resolve_type(function_type->parameters[i].type);
            const auto* argument_type = node.arguments[i]->value->type;
            if (expected_type == nullptr || argument_type == nullptr) {
                return false;
            }

            if (!argument_type->compatible(expected_type)) {
                if (emit_errors) {
                    context.diagnostics.add_error(node.arguments[i]->span,
                                                  "argument type mismatch: expected '" +
                                                      expected_type->to_string() + "', got '" +
                                                      argument_type->to_string() + "'");

                    all_valid = false;
                } else {
                    return false;
                }
            }
        }

        if (function_type->variadic && parameter_count > 0) {
            const auto* element_type =
                type_context.resolve_type(function_type->parameters.back().type);
            if (element_type == nullptr) {
                return false;
            }

            if (const auto* array_type = element_type->as<ArrayType>(); array_type != nullptr) {
                element_type = array_type->element;
            } else {
                if (emit_errors) {
                    context.diagnostics.add_error(node.span, "expected array type, got '" +
                                                                 element_type->to_string() + "'");
                    return false;
                }

                return false;
            }

            for (std::size_t i = required; i < node.arguments.size(); ++i) {
                const auto* actual_type = node.arguments[i]->value->type;
                if (actual_type == nullptr) {
                    return false;
                }

                if (!actual_type->compatible(element_type)) {
                    if (emit_errors) {
                        context.diagnostics.add_error(node.arguments[i]->span,
                                                      "argument type mismatch: expected '" +
                                                          element_type->to_string() + "', got '" +
                                                          actual_type->to_string() + "'");

                        all_valid = false;
                    }

                    return false;
                }
            }
        }

        return all_valid;
    }

  public:
    explicit CallResolver(Context& context, TypeContext& type_context, Visitor<void>& visitor)
        : visitor(visitor), context(context), type_context(type_context) {}

    const FunctionType* resolve_overload(const std::string& name, CallExpression& node) {
        for (auto* generic_argument : node.generic_arguments) {
            visitor.visit(*generic_argument);
        }

        const auto& overloads = context.env.current_scope->lookup_function(name);

        const FunctionSymbol* best_match = nullptr;
        int match_count = 0;

        for (const auto* symbol : overloads) {
            const auto* symbol_type = symbol->type;
            if (symbol_type == nullptr) {
                continue;
            }

            const auto* function_type = symbol_type->as<FunctionType>();
            if (function_type == nullptr) {
                continue;
            }

            if (is_valid(function_type, node, false)) {
                best_match = symbol;
                match_count = match_count + 1;
            }
        }

        if (match_count > 1) {
            context.diagnostics.add_error(node.span, "ambiguous call to '" + name + "'");
            return nullptr;
        }

        if (match_count == 1 && best_match != nullptr) {
            const auto* function_type = best_match->type;
            if (function_type == nullptr) {
                return nullptr;
            }

            return function_type->as<FunctionType>();
        }

        return nullptr;
    }

    bool is_valid(const FunctionType* function_type, CallExpression& node,
                  bool emit_errors = false) {
        GenericResolver generic_resolver(context, type_context, visitor);
        if (!generic_resolver.check_generic_arguments(node.generic_arguments,
                                                      function_type->generic_parameters, node.span,
                                                      emit_errors)) {
            return false;
        }

        if (!check_arguments(function_type, node, emit_errors)) {
            return false;
        }

        return true;
    }
};
