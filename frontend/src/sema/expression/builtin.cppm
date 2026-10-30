module;

#include <array>
#include <string>
#include <string_view>
#include <variant>

export module zep.frontend.sema.expression.builtin;

import zep.common.context;
import zep.frontend.node;
import zep.frontend.sema.type;
import zep.frontend.sema.type_resolver;

export class BuiltinChecker {
  private:
    Context& context;

    TypeResolver& resolver;
    const PrimitiveTypes& primitives;

    const Type* check_size(BuiltinCall& node) {
        const auto* resolved_type = resolver.resolve_type(node.type_argument->type);
        if (resolved_type == nullptr) {
            context.diagnostics.add_error(node.span, "cannot resolve type for '#sizeof'");
            return nullptr;
        }

        if (resolved_type->is<VoidType>()) {
            context.diagnostics.add_error(node.span, "'#sizeof' cannot be applied to void type");
            return nullptr;
        }

        if (const auto* named = resolved_type->as<NamedType>(); named != nullptr) {
            if (resolver.has_type_parameter(named->name)) {
                return primitives.entries.at("i64");
            }

            context.diagnostics.add_error(node.span,
                                          "'#sizeof' cannot be applied to unresolved type '" +
                                              resolved_type->to_string() + "'");
            return nullptr;
        }

        if (TypeLayout().resolve(resolved_type).state == TypeLayout::Result::State::Invalid) {
            context.diagnostics.add_error(node.span, "sizeof requires a concrete sized type");
            return nullptr;
        }

        return primitives.entries.at("i64");
    }

    const Type* check_length(BuiltinCall& node) {
        if (node.arguments.size() != 1) {
            context.diagnostics.add_error(node.span, "'#length' expects one argument");
            return nullptr;
        }

        const auto* argument_type = node.arguments[0]->type;
        if (argument_type == nullptr) {
            return nullptr;
        }

        const auto* resolved_type = resolver.resolve_type(argument_type);
        const auto* array_type =
            resolved_type != nullptr ? resolved_type->as<ArrayType>() : nullptr;

        if (array_type == nullptr ||
            std::holds_alternative<UnsizedArrayExtent>(array_type->extent)) {
            auto type_name =
                resolved_type != nullptr ? resolved_type->to_string() : std::string("unknown");
            context.diagnostics.add_error(node.arguments[0]->span,
                                          "'#length' requires a fixed-size array, got '" +
                                              type_name + "'");
            return nullptr;
        }

        return primitives.entries.at("i64");
    }

    const Type* check_asm(BuiltinCall& node) {
        if (node.arguments.size() != 1) {
            context.diagnostics.add_error(node.span, "'#asm' expects one string argument");
            return nullptr;
        }

        if (node.arguments[0]->as<StringLiteral>() == nullptr) {
            context.diagnostics.add_error(node.span, "'#asm' argument must be a string literal");
            return nullptr;
        }

        return primitives.entries.at("void");
    }

  public:
    static constexpr std::array<std::string_view, 3> names = {"sizeof", "length", "asm"};

    BuiltinChecker(Context& context, TypeResolver& resolver, const PrimitiveTypes& primitives)
        : context(context), resolver(resolver), primitives(primitives) {}

    const Type* check(BuiltinCall& node) {
        if (node.name == "sizeof") {
            return check_size(node);
        }

        if (node.name == "length") {
            return check_length(node);
        }

        if (node.name == "asm") {
            return check_asm(node);
        }

        context.diagnostics.add_error(node.span, "unknown builtin function '#" + node.name + "'");
        return nullptr;
    }
};
