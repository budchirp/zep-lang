module;

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <vector>

export module zep.frontend.sema.resolution.arguments;

import zep.common.context;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.types;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.expression.generic;

export class Arguments {
  public:
    static std::optional<std::vector<Argument*>> normalize(Context& context,
        const std::vector<Argument*>& arguments, std::span<const ParameterType> parameters,
        Span span, bool variadic, bool report) {
        if (arguments.size() != parameters.size() && !variadic) {
            if (report) {
                context.diagnostics.add_error(span,
                    std::string(arguments.size() < parameters.size() ? "too few" : "too many") +
                    " arguments: expected " + std::to_string(parameters.size()) +
                    ", got " + std::to_string(arguments.size()));
            }

            return std::nullopt;
        }

        std::vector<Argument*> normalized(parameters.size(), nullptr);
        normalized.reserve(std::max(parameters.size(), arguments.size()));
        std::unordered_set<std::string> names;
        names.reserve(arguments.size());
        auto named = false;
        auto valid = true;
        auto position = std::size_t{0};

        for (auto* argument : arguments) {
            if (argument->name.empty()) {
                if (named) {
                    if (report) {
                        context.diagnostics.add_error(argument->span,
                            "positional argument cannot follow named argument");
                    }

                    valid = false;
                    continue;
                }

                if (position < parameters.size()) {
                    normalized[position] = argument;
                    names.insert(parameters[position].name);
                    ++position;
                } else if (variadic) {
                    normalized.push_back(argument);
                }

                continue;
            }

            named = true;
            const auto found = std::ranges::find(parameters, argument->name, &ParameterType::name);

            if (found == parameters.end()) {
                if (report) {
                    context.diagnostics.add_error(argument->span, "unknown parameter '" + argument->name + "'");
                }

                valid = false;
                continue;
            }

            if (!names.insert(argument->name).second) {
                if (report) {
                    context.diagnostics.add_error(argument->span, "duplicate argument '" + argument->name + "'");
                }

                valid = false;
                continue;
            }

            normalized[static_cast<std::size_t>(found - parameters.begin())] = argument;
        }

        for (auto index = std::size_t{0}; index < parameters.size(); ++index) {
            if (normalized[index] != nullptr) {
                continue;
            }

            if (report) {
                context.diagnostics.add_error(span, "missing argument '" + parameters[index].name + "'");
            }

            valid = false;
        }

        return valid ? std::optional(std::move(normalized)) : std::nullopt;
    }

    static std::vector<GenericArgument*> infer(Context& context, SemaContext& sema,
        TypeResolver& resolver, const std::vector<GenericParameterType>& generics,
        std::span<const ParameterType> parameters, const std::vector<Argument*>& arguments,
        Span span, bool report = false) {
        GenericInference inference(context, sema, resolver, generics);
        const auto count = std::min(parameters.size(), arguments.size());

        for (auto index = std::size_t{0}; index < count; ++index) {
            const auto* value = arguments[index]->value;

            if (value != nullptr && value->type != nullptr) {
                inference.match(parameters[index].type, value->type);
            }
        }

        return inference.arguments(span, report);
    }
};
