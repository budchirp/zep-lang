module;

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

export module zep.frontend.sema.resolver.generic;

import zep.common.context;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.type;
import zep.frontend.sema.type.resolver;
import zep.frontend.sema.constant.evaluator;

export class GenericArgumentResolver {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    Visitor<void>& visitor;

    static void contextualize_literal(Expression& expression, const Type* type) {
        if (expression.as<NumberLiteral>() != nullptr || expression.as<FloatLiteral>() != nullptr) {
            expression.type = type;
            expression.compile_time_value.reset();
        } else if (auto* unary = expression.as<UnaryExpression>();
                   unary != nullptr && (unary->op == UnaryExpression::Operator::Type::Plus ||
                                        unary->op == UnaryExpression::Operator::Type::Minus)) {
            contextualize_literal(*unary->operand, type);
            expression.type = type;
            expression.compile_time_value.reset();
        }
    }

    std::optional<GenericBinding> resolve(GenericArgument& argument,
                                          const GenericParameterType& parameter, bool emit_errors) {
        argument.const_binding.reset();
        if (parameter.is_const() && argument.value == nullptr && argument.type != nullptr) {
            const auto* named = argument.type->source_type->as<NamedType>();
            if (named != nullptr && named->generic_arguments.empty()) {
                argument.value =
                    sema.nodes.create<IdentifierExpression>(argument.span, named->name);
            }
        }
        visitor.visit(argument);

        if (!parameter.is_const()) {
            const auto* type = argument.type != nullptr ? argument.type->type : nullptr;
            if (argument.value != nullptr || type == nullptr ||
                (parameter.type != nullptr &&
                 !resolver.satisfies_constraint(type, parameter.type))) {
                if (emit_errors) {
                    context.diagnostics.add_error(
                        argument.span, "generic argument does not satisfy type parameter '" +
                                           parameter.name + "'");
                }
                return std::nullopt;
            }
            return TypeBinding(type);
        }

        if (argument.value == nullptr) {
            if (emit_errors) {
                context.diagnostics.add_error(argument.span, "expected const generic argument");
            }
            return std::nullopt;
        }

        const auto* target = resolver.resolve_type(parameter.type);
        if (target != nullptr && target->is_numeric()) {
            contextualize_literal(*argument.value, target);
        }
        Evaluator evaluator(context.diagnostics, &sema.env, &resolver.environment(), true);
        const auto value = evaluator.evaluate_uncached(*argument.value);
        const auto* identifier = argument.value->as<IdentifierExpression>();
        const auto candidate = value.has_value() ? ConstBinding(*value)
                               : identifier != nullptr && identifier->generic_declaration != nullptr
                                   ? ConstBinding(identifier->generic_declaration, target, false)
                                   : ConstBinding(argument.value, target, true);
        const auto binding = resolver.normalize_const_binding(candidate, target, argument.span);
        if (!binding.has_value()) {
            return std::nullopt;
        }

        argument.const_binding = *binding;
        return *argument.const_binding;
    }

  public:
    GenericArgumentResolver(Context& context, SemaContext& sema, TypeResolver& resolver,
                            Visitor<void>& visitor)
        : context(context), sema(sema), resolver(resolver), visitor(visitor) {}

    std::optional<std::vector<GenericBinding>>
    resolve_function_arguments(const std::vector<GenericArgument*>& arguments,
                               const std::vector<GenericParameterType>& parameters, Span span) {
        if (arguments.size() != parameters.size()) {
            context.diagnostics.add_error(span, "incorrect number of generic arguments");
            return std::nullopt;
        }
        std::vector<std::optional<GenericBinding>> ordered(parameters.size());
        auto seen_named = false;
        for (std::size_t index = 0; index < arguments.size(); ++index) {
            auto* argument = arguments[index];
            if (argument->name.empty() && seen_named) {
                context.diagnostics.add_error(
                    argument->span,
                    "positional generic argument cannot follow named generic argument");
                return std::nullopt;
            }
            auto position = index;
            if (!argument->name.empty()) {
                seen_named = true;
                const auto found =
                    std::ranges::find(parameters, argument->name, &GenericParameterType::name);
                if (found == parameters.end()) {
                    context.diagnostics.add_error(argument->span, "unknown generic argument '" +
                                                                      argument->name + "'");
                    return std::nullopt;
                }
                position = static_cast<std::size_t>(found - parameters.begin());
            }
            if (ordered[position].has_value()) {
                context.diagnostics.add_error(argument->span, "duplicate generic argument '" +
                                                                  parameters[position].name + "'");
                return std::nullopt;
            }
            ordered[position] = resolve(*argument, parameters[position], true);
            if (!ordered[position].has_value()) {
                return std::nullopt;
            }
        }
        std::vector<GenericBinding> result;
        result.reserve(ordered.size());
        for (auto& binding : ordered) {
            result.push_back(std::move(*binding));
        }
        return result;
    }

    std::optional<std::vector<GenericArgumentType>>
    resolve_type_arguments(const std::vector<GenericArgument*>& arguments,
                           const NominalType* receiver, Span span) {
        if (receiver != nullptr && arguments.size() != receiver->generic_parameters.size()) {
            context.diagnostics.add_error(span, "incorrect number of generic arguments");
            return std::nullopt;
        }

        std::vector<GenericArgumentType> result;
        result.reserve(arguments.size());
        std::vector<bool> bound(arguments.size(), false);
        auto seen_named = false;
        for (std::size_t index = 0; index < arguments.size(); ++index) {
            auto* argument = arguments[index];
            if (seen_named && argument->name.empty()) {
                context.diagnostics.add_error(
                    argument->span,
                    "positional generic argument cannot follow named generic argument");
                return std::nullopt;
            }
            seen_named = !argument->name.empty();

            if (receiver != nullptr) {
                const auto& parameters = receiver->generic_parameters;
                auto parameter_index = index;
                if (!argument->name.empty()) {
                    const auto found =
                        std::ranges::find(parameters, argument->name, &GenericParameterType::name);
                    if (found == parameters.end()) {
                        context.diagnostics.add_error(argument->span, "unknown generic argument '" +
                                                                          argument->name + "'");
                        return std::nullopt;
                    }
                    parameter_index = static_cast<std::size_t>(found - parameters.begin());
                }

                if (bound[parameter_index]) {
                    context.diagnostics.add_error(argument->span,
                                                  "duplicate generic argument '" +
                                                      parameters[parameter_index].name + "'");
                    return std::nullopt;
                }
                bound[parameter_index] = true;

                const auto binding = resolve(*argument, parameters[parameter_index], true);
                if (!binding.has_value()) {
                    return std::nullopt;
                }
                result.emplace_back(argument->name, *binding);
                continue;
            }

            if (argument->type != nullptr && argument->value == nullptr) {
                const auto* named = argument->type->source_type->as<NamedType>();
                if (named != nullptr && resolver.const_parameter(named->name).has_value()) {
                    argument->value =
                        sema.nodes.create<IdentifierExpression>(argument->span, named->name);
                }
            }
            visitor.visit(*argument);
            if (argument->value != nullptr) {
                Evaluator evaluator(context.diagnostics, &sema.env, &resolver.environment(), true);
                const auto value = evaluator.evaluate_uncached(*argument->value);
                const auto* identifier = argument->value->as<IdentifierExpression>();
                argument->const_binding =
                    value.has_value() ? ConstBinding(*value)
                    : identifier != nullptr && identifier->generic_declaration != nullptr
                        ? ConstBinding(identifier->generic_declaration, argument->value->type,
                                       false)
                        : ConstBinding(argument->value, argument->value->type, true);
                result.emplace_back(argument->name, *argument->const_binding);
            } else {
                result.emplace_back(argument->name, argument->type->type);
            }
        }
        return result;
    }

    bool apply(const std::vector<GenericArgument*>& arguments,
               const std::vector<GenericParameterType>& parameters, bool emit_errors) {
        std::vector<GenericBinding> bindings;
        bindings.reserve(arguments.size());
        for (std::size_t index = 0; index < arguments.size(); ++index) {
            auto binding = resolve(*arguments[index], parameters[index], emit_errors);
            if (!binding.has_value()) {
                return false;
            }
            bindings.push_back(std::move(*binding));
        }

        for (std::size_t index = 0; index < bindings.size(); ++index) {
            resolver.bind_generic_binding(parameters[index].name, bindings[index],
                                          parameters[index].declaration);
        }
        return true;
    }
};
