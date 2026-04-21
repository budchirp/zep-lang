module;

#include <string>
#include <vector>

export module zep.frontend.sema.resolver.call;

import zep.frontend.sema.type;
import zep.frontend.sema.type.type_id;
import zep.frontend.sema.type.type_helper;
import zep.frontend.ast;
import zep.common.logger.diagnostic;
import zep.frontend.sema.type.type_context;
import zep.frontend.sema.context;
import zep.frontend.sema.resolver.generic;
import zep.frontend.sema.symbol;
export class CallResolver {
  private:
    Context& context;
    TypeContext& type_context;
    Visitor<void>& visitor;

    bool check_arguments(const FunctionType* candidate_type, CallExpression& node,
                         bool emit_errors = false) {
        std::size_t parameter_count = candidate_type->parameters.size();
        std::size_t required =
            candidate_type->variadic && parameter_count > 0 ? parameter_count - 1 : parameter_count;

        if (candidate_type->variadic) {
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
                context.diagnostics.add_error(node.span,
                                              "expected " + std::to_string(parameter_count) +
                                                  " argument(s), got " +
                                                  std::to_string(node.arguments.size()));
            }
            return false;
        }

        bool all_valid = true;

        for (std::size_t i = 0; i < required; ++i) {
            TypeId expected_type =
                type_context.resolve_type(candidate_type->parameters[i].type);
            TypeId actual_type = node.arguments[i]->value->type;

            if (!actual_type.is_valid() ||
                !context.type_helper.compatible( actual_type, expected_type)) {
                if (emit_errors) {
                    context.diagnostics.add_error(
                        node.arguments[i]->span,
                        "argument type mismatch: expected '" +
                            context.type_helper.type_to_string( expected_type) + "', got '" +
                            (actual_type.is_valid() ? context.type_helper.type_to_string( actual_type)
                                                    : "unknown") +
                            "'");
                    all_valid = false;
                } else {
                    return false;
                }
            }
        }

        if (candidate_type->variadic && parameter_count > 0) {
            TypeId element_type =
                type_context.resolve_type(candidate_type->parameters.back().type);
            if (!element_type.is_valid()) {
                return false;
            }

            const Type* element_type_ptr = context.types.get(element_type);
            if (element_type_ptr == nullptr) {
                return false;
            }

            const ArrayType* array_type = element_type_ptr->as<ArrayType>();
            if (array_type != nullptr) {
                element_type = array_type->element;
            } else if (emit_errors) {
                context.diagnostics.add_error(node.span, "expected array type, got '" +
                                                             context.type_helper.type_to_string(
                                                                            element_type) +
                                                             "'");
                return false;
            } else {
                return false;
            }

            for (std::size_t i = required; i < node.arguments.size(); ++i) {
                TypeId actual_type = node.arguments[i]->value->type;
                if (!actual_type.is_valid() ||
                    !context.type_helper.compatible( actual_type, element_type)) {
                    if (emit_errors) {
                        context.diagnostics.add_error(
                            node.arguments[i]->span,
                            "argument type mismatch: expected '" +
                                context.type_helper.type_to_string( element_type) + "', got '" +
                                (actual_type.is_valid()
                                     ? context.type_helper.type_to_string( actual_type)
                                     : "unknown") +
                                "'");
                        all_valid = false;
                    } else {
                        return false;
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
        for (GenericArgument* generic_argument : node.generic_arguments) {
            visitor.visit(*generic_argument);
        }

        std::vector<const FunctionSymbol*> overloads =
            context.env.current_scope->lookup_function_overloads(name);

        const FunctionSymbol* best_match = nullptr;
        int match_count = 0;

        for (const FunctionSymbol* symbol : overloads) {
            const Type* candidate_type_ptr = context.types.get(symbol->type);
            if (candidate_type_ptr == nullptr) {
                continue;
            }

            const FunctionType* candidate = candidate_type_ptr->as<FunctionType>();
            if (candidate == nullptr) {
                continue;
            }

            TypeContext::SubstitutionScope substitution_scope = type_context.scoped_substitutions();
            (void)substitution_scope;

            if (is_valid(candidate, node, false)) {
                best_match = symbol;
                match_count = match_count + 1;
            }
        }

        if (match_count > 1) {
            context.diagnostics.add_error(node.span, "ambiguous call to '" + name + "'");
            return nullptr;
        }

        if (match_count == 1 && best_match != nullptr) {
            const Type* best_match_type = context.types.get(best_match->type);
            if (best_match_type == nullptr) {
                return nullptr;
            }

            return best_match_type->as<FunctionType>();
        }

        return nullptr;
    }

    bool is_valid(const FunctionType* candidate_type, CallExpression& node,
                  bool emit_errors = false) {
        GenericResolver generic_resolver(context, type_context, visitor);
        if (!generic_resolver.check_generic_arguments(node.generic_arguments,
                                                      candidate_type->generic_parameters,
                                                      node.span, emit_errors)) {
            return false;
        }

        if (!check_arguments(candidate_type, node, emit_errors)) {
            return false;
        }

        return true;
    }
};
