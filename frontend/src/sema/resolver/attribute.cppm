module;

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.frontend.sema.resolver.attribute;

import zep.common.context;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.env;
import zep.frontend.sema.scope;
import zep.common.target;

export class AttributeResolver {
  public:
    class ValidationContext {
      public:
        SemaContext& sema;
        Context& context;

        Span span;

        std::string name;
        std::vector<GenericParameter*> generic_parameters;
    };

  private:
    using Handler = void (*)(const AttributeInfo&, const ValidationContext&);

    static void reject_generic_or_overloaded_function_link(const ValidationContext& ctx,
                                                           std::string_view prefix) {
        if (!ctx.generic_parameters.empty()) {
            ctx.context.diagnostics.add_error(
                ctx.span, std::string(prefix) + " cannot be used with generic functions");
            return;
        }

        if (!ctx.sema.env.current_scope->lookup_function_overloads(ctx.name).empty()) {
            ctx.context.diagnostics.add_error(ctx.span, std::string(prefix) + " function '" +
                                                            ctx.name + "' cannot be overloaded");
        }
    }

    static void handle_mangle(const AttributeInfo& attribute, const ValidationContext& ctx) {
        if (!attribute.arguments.empty() && attribute.arguments[0] == "false") {
            reject_generic_or_overloaded_function_link(ctx, "'@mangle(false)'");
        }
    }

    static void handle_name(const AttributeInfo& attribute, const ValidationContext& ctx) {
        if (attribute.arguments.size() != 1) {
            ctx.context.diagnostics.add_error(
                ctx.span, "'@name' attribute requires exactly one string argument");
            return;
        }

        reject_generic_or_overloaded_function_link(ctx, "'@name'");
    }

    static void handle_os(const AttributeInfo& attribute, const ValidationContext& ctx) {
        if (attribute.arguments.empty()) {
            ctx.context.diagnostics.add_error(ctx.span,
                                              "'@os' attribute requires at least one argument");
            return;
        }

        for (const auto& argument : attribute.arguments) {
            if (TargetOS::from(argument) == TargetOS::Kind::Type::Unknown) {
                ctx.context.diagnostics.add_error(ctx.span,
                                                  "'@os' attribute expects 'linux' or 'macos'");
                return;
            }
        }
    }

    static void handle_arch(const AttributeInfo& attribute, const ValidationContext& ctx) {
        if (attribute.arguments.empty()) {
            ctx.context.diagnostics.add_error(ctx.span,
                                              "'@arch' attribute requires at least one argument");
            return;
        }

        for (const auto& argument : attribute.arguments) {
            if (TargetArch::from(argument) == TargetArch::Kind::Type::Unknown) {
                ctx.context.diagnostics.add_error(
                    ctx.span, "'@arch' attribute expects 'x86_64' or 'aarch64'");
                return;
            }
        }
    }

    static void handle_section(const AttributeInfo& attribute, const ValidationContext& ctx) {
        if (attribute.arguments.empty()) {
            ctx.context.diagnostics.add_error(ctx.span,
                                              "'@section' attribute requires a string argument");
            return;
        }
    }

    static void handle_align(const AttributeInfo& attribute, const ValidationContext& ctx) {
        if (attribute.arguments.empty()) {
            ctx.context.diagnostics.add_error(ctx.span,
                                              "'@align' attribute requires a numeric argument");
            return;
        }
    }

    static const std::unordered_map<std::string, Handler>& handler_map() {
        static const std::unordered_map<std::string, Handler> map = {
            {"mangle", handle_mangle}, {"name", handle_name},       {"os", handle_os},
            {"arch", handle_arch},     {"section", handle_section}, {"align", handle_align},
        };

        return map;
    }

    static void validate_one(const AttributeInfo& attribute, const ValidationContext& ctx) {
        const auto& map = handler_map();
        const auto iterator = map.find(attribute.name);
        if (iterator != map.end()) {
            iterator->second(attribute, ctx);
        }
    }

  public:
    static std::vector<AttributeInfo> convert(const std::vector<Attribute*>& attributes) {
        std::vector<AttributeInfo> result;
        result.reserve(attributes.size());

        for (const auto* attribute : attributes) {
            std::vector<std::string> string_arguments;
            string_arguments.reserve(attribute->arguments.size());

            for (const auto* argument : attribute->arguments) {
                if (const auto* string_literal = argument->as<StringLiteral>();
                    string_literal != nullptr) {
                    string_arguments.push_back(string_literal->value);
                } else if (const auto* boolean_literal = argument->as<BooleanLiteral>();
                           boolean_literal != nullptr) {
                    string_arguments.push_back(boolean_literal->value ? "true" : "false");
                } else if (const auto* identifier = argument->as<IdentifierExpression>();
                           identifier != nullptr) {
                    string_arguments.push_back(identifier->name);
                } else if (const auto* number_literal = argument->as<NumberLiteral>();
                           number_literal != nullptr) {
                    string_arguments.push_back(number_literal->value);
                }
            }

            result.emplace_back(attribute->name, std::move(string_arguments));
        }

        return result;
    }

    static void validate(const std::vector<AttributeInfo>& attributes,
                         const ValidationContext& ctx) {
        for (const auto& attribute : attributes) {
            validate_one(attribute, ctx);
        }
    }

    static std::string get_linker_name(const std::string& original_name,
                                       const std::vector<AttributeInfo>& attributes) {
        for (const auto& attribute : attributes) {
            if (attribute.name == "name") {
                if (!attribute.arguments.empty()) {
                    return attribute.arguments[0];
                }
            }
        }
        return original_name;
    }

    static bool is_target_active(const std::vector<Attribute*>& attributes,
                                 const TargetInfo& target) {
        auto os_matches = true;
        auto arch_matches = true;

        for (auto* attribute : attributes) {
            if (attribute == nullptr) {
                continue;
            }

            if (attribute->name == "os") {
                os_matches = matches_os_target(attribute->arguments, target.os);
            } else if (attribute->name == "arch") {
                arch_matches = matches_arch_target(attribute->arguments, target.arch);
            }
        }

        return os_matches && arch_matches;
    }

  private:
    static bool matches_os_target(const std::vector<Expression*>& arguments,
                                  TargetOS::Kind::Type expected) {
        if (arguments.empty()) {
            return false;
        }

        for (auto* argument : arguments) {
            auto value = argument_value(argument);
            if (TargetOS::from(value) == expected) {
                return true;
            }
        }

        return false;
    }

    static bool matches_arch_target(const std::vector<Expression*>& arguments,
                                    TargetArch::Kind::Type expected) {
        if (arguments.empty()) {
            return false;
        }

        for (auto* argument : arguments) {
            auto value = argument_value(argument);
            if (TargetArch::from(value) == expected) {
                return true;
            }
        }

        return false;
    }

    static std::string argument_value(Expression* expression) {
        if (expression == nullptr) {
            return "";
        }

        if (auto* string_literal = expression->as<StringLiteral>(); string_literal != nullptr) {
            return string_literal->value;
        }

        if (auto* identifier = expression->as<IdentifierExpression>(); identifier != nullptr) {
            return identifier->name;
        }

        return "";
    }
};
