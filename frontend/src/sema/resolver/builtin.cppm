module;

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

export module zep.frontend.sema.resolver.builtin;

import zep.common.context;
import zep.frontend.node;
import zep.frontend.sema.type;
import zep.frontend.sema.type.resolver;

export class BuiltinResolver {
  private:
    using Handler = const Type* (*)(BuiltinCall&, Context&, TypeResolver&, const BuiltinResolver&);

    static bool permits_size(const Type* type, TypeResolver& resolver,
                             std::unordered_set<const Type*>& active) {
        if (type == nullptr || type->is<VoidType>() || type->is<NeverType>() ||
            !active.insert(type).second) {
            return false;
        }

        auto permitted = true;
        if (const auto* named = type->as<NamedType>(); named != nullptr) {
            permitted = resolver.has_type_parameter(named->name);
        } else if (const auto* array = type->as<ArrayType>(); array != nullptr) {
            permitted = !std::holds_alternative<UnsizedArrayExtent>(array->extent) &&
                        permits_size(array->element, resolver, active);
        } else if (const auto* structure = type->as<StructType>(); structure != nullptr) {
            for (const auto& field : structure->fields) {
                permitted = permits_size(field.type, resolver, active) && permitted;
            }
        } else if (const auto* enumeration = type->as<EnumType>(); enumeration != nullptr) {
            if (enumeration->backing_type != nullptr) {
                permitted = permits_size(enumeration->backing_type, resolver, active);
            }

            for (const auto& variant : enumeration->variants) {
                for (const auto& field : variant.fields) {
                    permitted = permits_size(field.type, resolver, active) && permitted;
                }
            }
        }

        active.erase(type);
        return permitted;
    }

    static const Type* handle_sizeof(BuiltinCall& node, Context& context, TypeResolver& resolver,
                                     const BuiltinResolver& self) {
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
                return self.primitives.at("i64");
            }

            context.diagnostics.add_error(node.span,
                                          "'#sizeof' cannot be applied to unresolved type '" +
                                              resolved_type->to_string() + "'");
            return nullptr;
        }

        std::unordered_set<const Type*> active;
        if (!permits_size(resolved_type, resolver, active)) {
            context.diagnostics.add_error(node.span, "sizeof requires a concrete sized type");
            return nullptr;
        }

        return self.primitives.at("i64");
    }

    static const Type* handle_length(BuiltinCall& node, Context& context, TypeResolver& resolver,
                                     const BuiltinResolver& self) {
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

        if (array_type == nullptr) {
            auto type_name =
                resolved_type != nullptr ? resolved_type->to_string() : std::string("unknown");
            context.diagnostics.add_error(node.arguments[0]->span,
                                          "'#length' requires a fixed-size array, got '" +
                                              type_name + "'");
            return nullptr;
        }

        if (std::holds_alternative<UnsizedArrayExtent>(array_type->extent)) {
            auto type_name =
                resolved_type != nullptr ? resolved_type->to_string() : std::string("unknown");
            context.diagnostics.add_error(node.arguments[0]->span,
                                          "'#length' requires a fixed-size array, got '" +
                                              type_name + "'");
            return nullptr;
        }

        return self.primitives.at("i64");
    }

    static const Type* handle_asm(BuiltinCall& node, Context& context,
                                  [[maybe_unused]] TypeResolver& resolver,
                                  const BuiltinResolver& self) {
        if (node.arguments.size() != 1) {
            context.diagnostics.add_error(node.span, "'#asm' expects one string argument");
            return nullptr;
        }

        if (node.arguments[0]->as<StringLiteral>() == nullptr) {
            context.diagnostics.add_error(node.span, "'#asm' argument must be a string literal");
            return nullptr;
        }

        return self.primitives.at("void");
    }

    static const std::unordered_map<std::string, Handler>& handler_map() {
        static const std::unordered_map<std::string, Handler> map = {
            {"sizeof", handle_sizeof},
            {"length", handle_length},
            {"asm", handle_asm},
        };

        return map;
    }

  public:
    std::unordered_map<std::string, const Type*> primitives;

    explicit BuiltinResolver(TypeArena& type_arena) {
        primitives.reserve(16);
        primitives["void"] = type_arena.create<VoidType>();
        primitives["never"] = type_arena.create<NeverType>();
        primitives["cstr"] = type_arena.create<StringType>();
        primitives["boolean"] = type_arena.create<BooleanType>();
        primitives["char"] = type_arena.create<CharType>();
        primitives["any"] = type_arena.create<AnyType>();

        for (auto size : {8, 16, 32, 64}) {
            auto bits = static_cast<std::uint8_t>(size);
            primitives["i" + std::to_string(size)] = type_arena.create<IntegerType>(false, bits);
            primitives["u" + std::to_string(size)] = type_arena.create<IntegerType>(true, bits);
        }

        for (auto size : {32, 64}) {
            auto bits = static_cast<std::uint8_t>(size);
            primitives["f" + std::to_string(size)] = type_arena.create<FloatType>(bits);
        }
    }

    ~BuiltinResolver() = default;

    static std::vector<std::string> builtin_names() {
        const auto& map = handler_map();
        std::vector<std::string> names;
        names.reserve(map.size());
        for (const auto& [name, _] : map) {
            names.push_back("#" + name);
        }
        return names;
    }

    bool is_builtin(const std::string& name) const { return handler_map().contains(name); }

    const Type* check(const std::string& name, BuiltinCall& node, Context& context,
                      TypeResolver& resolver) const {
        const auto& map = handler_map();
        const auto iterator = map.find(name);
        if (iterator != map.end()) {
            return iterator->second(node, context, resolver, *this);
        }

        context.diagnostics.add_error(node.span, "unknown builtin function '#" + name + "'");
        return nullptr;
    }
};
