module;

#include <algorithm>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

export module zep.frontend.sema.expression.generic;

import zep.common.context;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.types;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.constants.evaluator;
import zep.frontend.sema.expression.values;

export class GenericInference {
  private:
    SemaContext& sema;
    TypeResolver& resolver;
    const std::vector<GenericParameterType>& parameters;
    std::vector<const Type*> inferred;
    std::unordered_set<std::size_t> literals;
    std::string failure;

  public:
    class Source {
      public:
        enum class Type : std::uint8_t { Argument, Expected, Literal };
    };

    GenericInference(SemaContext& sema, TypeResolver& resolver,
                     const std::vector<GenericParameterType>& parameters)
        : sema(sema), resolver(resolver), parameters(parameters),
          inferred(parameters.size(), nullptr), literals(), failure() {
        literals.reserve(parameters.size());
    }

    void match(const Type* declared, const Type* actual,
               Source::Type origin = Source::Type::Argument) {
        if (declared == nullptr || actual == nullptr || !failure.empty()) {
            return;
        }

        if (resolver.is_concrete(actual)) {
            actual = resolver.resolve_type(actual);
        }
        if (actual == nullptr) {
            return;
        }

        if (const auto* named = declared->as<NamedType>();
            named != nullptr && named->generic_arguments.empty() && named->declaration != nullptr) {
            for (auto index = std::size_t{0}; index < parameters.size(); ++index) {
                const auto& parameter = parameters[index];
                if (parameter.is_const() || parameter.declaration != named->declaration) {
                    continue;
                }

                const auto* previous = resolver.constraint_type(inferred[index]);
                const auto* candidate = resolver.constraint_type(actual);
                const auto same_number_family = previous != nullptr && candidate != nullptr &&
                    previous->kind == candidate->kind &&
                    (previous->is<IntegerType>() || previous->is<FloatType>());

                if (inferred[index] == nullptr) {
                    inferred[index] = actual;

                    if (origin == Source::Type::Literal && resolver.is_concrete(actual)) {
                        literals.insert(index);
                    }
                } else if (origin == Source::Type::Expected && literals.contains(index) && same_number_family) {
                    inferred[index] = actual;
                } else if (origin != Source::Type::Expected && !inferred[index]->same(actual) &&
                           !(origin == Source::Type::Literal && same_number_family)) {
                    failure = "conflicting inferred types for generic parameter '" + parameter.name +
                              "': '" + inferred[index]->to_string() + "' and '" +
                              actual->to_string() + "'";
                }
                return;
            }
        }

        declared = resolver.resolve_type(declared);
        if (declared == nullptr) {
            return;
        }

        if (const auto* pointer = declared->as<PointerType>(); pointer != nullptr) {
            if (pointer->reference_kind != PointerType::ReferenceKind::Type::None) {
                const auto* source = actual->as<PointerType>();
                const auto* value = source != nullptr && source->reference_kind != PointerType::ReferenceKind::Type::None
                    ? source->element : actual;
                match(pointer->element, value, origin);
                return;
            }
            if (const auto* actual_pointer = actual->as<PointerType>(); actual_pointer != nullptr) {
                match(pointer->element, actual_pointer->element, origin);
            }
            return;
        }

        if (const auto* array = declared->as<ArrayType>(); array != nullptr) {
            if (const auto* actual_array = actual->as<ArrayType>(); actual_array != nullptr) {
                match(array->element, actual_array->element, origin);
            }
            return;
        }

        if (const auto* function = declared->as<FunctionType>(); function != nullptr) {
            const auto* actual_function = actual->as<FunctionType>();
            if (actual_function == nullptr || function->variadic != actual_function->variadic) {
                return;
            }

            const auto parameters = function->value_parameters();
            const auto actual_parameters = actual_function->value_parameters();

            if (parameters.size() != actual_parameters.size()) {
                return;
            }

            match(function->return_type, actual_function->return_type, origin);
            for (auto index = std::size_t{0}; index < parameters.size(); ++index) {
                match(parameters[index].type, actual_parameters[index].type, origin);
            }
            return;
        }

        const auto* nominal = declared->as_nominal();
        const auto* actual_nominal = actual->as_nominal();
        if (nominal == nullptr || actual_nominal == nullptr ||
            nominal->definition != actual_nominal->definition ||
            nominal->generic_arguments.size() != actual_nominal->generic_arguments.size()) {
            return;
        }

        for (auto index = std::size_t{0}; index < nominal->generic_arguments.size(); ++index) {
            const auto& argument = nominal->generic_arguments[index];
            const auto& actual_argument = actual_nominal->generic_arguments[index];
            if (!argument.is_const() && !actual_argument.is_const()) {
                match(argument.type, actual_argument.type, origin);
            }
        }
    }

    std::expected<std::vector<GenericArgument*>, std::string> arguments(Span span) {
        if (!failure.empty()) {
            return std::unexpected(failure);
        }

        std::vector<GenericArgument*> result;
        result.reserve(parameters.size());
        for (auto index = std::size_t{0}; index < parameters.size(); ++index) {
            if (inferred[index] == nullptr) {
                return std::unexpected("cannot infer generic parameter '" + parameters[index].name +
                                       "'; specify generic arguments explicitly");
            }

            auto* expression = sema.nodes.create<TypeExpression>(span, inferred[index]);
            result.push_back(sema.nodes.create<GenericArgument>(
                span, parameters[index].name, expression));
        }
        return result;
    }
};

export class GenericArgumentResolver {
  private:
    Context& context;
    SemaContext& sema;
    TypeResolver& resolver;
    Visitor<void>& visitor;

    static void contextualize_literal(Expression& expression, const Type* type) {
        if (!Values::numeric_literal(expression)) {
            return;
        }

        if (auto* unary = expression.as<UnaryExpression>(); unary != nullptr) {
            contextualize_literal(*unary->operand, type);
        }

        expression.type = type;
        expression.compile_time_value.reset();
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
        Evaluator evaluator(context.diagnostics, &sema.env, &resolver.substitutions,
                            EvaluationPolicy::Type::Dependent);
        const auto value = evaluator.evaluate(*argument.value).value;
        const auto* identifier = argument.value->as<IdentifierExpression>();
        const auto candidate = value.has_value() ? ConstBinding(*value)
                               : identifier != nullptr && identifier->generic_declaration != nullptr
                                   ? ConstBinding::dependent_parameter(
                                         identifier->generic_declaration, target)
                                   : ConstBinding::dependent_expression(argument.value, target);
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
                Evaluator evaluator(context.diagnostics, &sema.env, &resolver.substitutions,
                                    EvaluationPolicy::Type::Dependent);
                const auto value = evaluator.evaluate(*argument->value).value;
                const auto* identifier = argument->value->as<IdentifierExpression>();
                argument->const_binding =
                    value.has_value() ? ConstBinding(*value)
                    : identifier != nullptr && identifier->generic_declaration != nullptr
                        ? ConstBinding::dependent_parameter(identifier->generic_declaration,
                                                            argument->value->type)
                        : ConstBinding::dependent_expression(argument->value,
                                                             argument->value->type);
                result.emplace_back(argument->name, *argument->const_binding);
            } else {
                result.emplace_back(argument->name, argument->type->type);
            }
        }
        return result;
    }

    bool apply(std::vector<GenericArgument*>& arguments,
               const std::vector<GenericParameterType>& parameters, Span span, bool emit_errors) {
        if (arguments.size() != parameters.size()) {
            if (emit_errors) {
                context.diagnostics.add_error(
                    span, "expected " + std::to_string(parameters.size()) +
                              " generic argument(s), got " + std::to_string(arguments.size()));
            }
            return false;
        }

        std::vector<GenericArgument*> ordered(parameters.size(), nullptr);
        std::unordered_set<std::string> used_names;
        used_names.reserve(arguments.size());

        auto seen_named = false;
        auto valid = true;
        auto positional_index = std::size_t{0};

        for (auto* argument : arguments) {
            if (argument->name.empty()) {
                if (seen_named) {
                    if (emit_errors) {
                        context.diagnostics.add_error(
                            argument->span,
                            "positional generic argument cannot follow named generic argument");
                    }
                    valid = false;
                    continue;
                }

                ordered[positional_index] = argument;
                used_names.insert(parameters[positional_index].name);
                ++positional_index;
                continue;
            }

            seen_named = true;
            const auto parameter =
                std::ranges::find(parameters, argument->name, &GenericParameterType::name);
            if (parameter == parameters.end()) {
                if (emit_errors) {
                    context.diagnostics.add_error(argument->span, "unknown generic argument '" +
                                                                      argument->name + "'");
                }
                valid = false;
                continue;
            }

            if (!used_names.insert(argument->name).second) {
                if (emit_errors) {
                    context.diagnostics.add_error(argument->span, "duplicate generic argument '" +
                                                                      argument->name + "'");
                }
                valid = false;
                continue;
            }

            ordered[static_cast<std::size_t>(parameter - parameters.begin())] = argument;
        }

        for (auto index = std::size_t{0}; index < parameters.size(); ++index) {
            if (ordered[index] != nullptr) {
                continue;
            }

            if (emit_errors) {
                context.diagnostics.add_error(span, "missing generic argument '" +
                                                        parameters[index].name + "'");
            }
            valid = false;
        }

        if (!valid) {
            return false;
        }

        arguments = std::move(ordered);

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
