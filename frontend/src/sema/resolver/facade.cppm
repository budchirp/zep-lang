module;

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.frontend.sema.resolver.facade;

import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
import zep.frontend.sema.constant.environment;
import zep.frontend.sema.type.resolver;

export class FacadeResolver {
  private:
    SemaContext& sema;
    TypeResolver& type_resolver;

    const PrimitiveFacadeInfo* lookup_info_by_name(const std::string& name) const {
        if (const auto* info = sema.facades.find_by_name(name); info != nullptr) {
            return info;
        }

        auto dot = name.rfind('.');
        auto colon = name.rfind("::");
        auto position = dot;
        if (colon != std::string::npos && (position == std::string::npos || colon > position)) {
            position = colon + 1;
        }

        if (position != std::string::npos && position + 1 < name.size()) {
            auto local_name = name.substr(position + 1);
            return sema.facades.find_by_name(local_name);
        }

        return nullptr;
    }

    const PrimitiveFacadeInfo* lookup_info_by_backing(const Type* type) const {
        const auto* resolved = type_resolver.resolve_type(type);
        if (resolved == nullptr) {
            return nullptr;
        }

        if (const auto* info = sema.facades.find_by_backing(resolved); info != nullptr) {
            return info;
        }

        for (const auto& entry : sema.facades.all()) {
            const auto& info = entry.second;
            if (!info.is_generic() || info.backing_constraint == nullptr) {
                continue;
            }

            if (type_resolver.satisfies_constraint(resolved, info.backing_constraint)) {
                return &info;
            }
        }

        return nullptr;
    }

    const Type* resolve_generic_backing(const PrimitiveFacadeInfo& info,
                                        const StructType& type) const {
        if (!info.is_generic() || info.type == nullptr) {
            return nullptr;
        }

        for (std::size_t index = 0; index < info.type->generic_parameters.size(); ++index) {
            if (info.type->generic_parameters[index].name != info.backing_parameter) {
                continue;
            }

            if (index >= type.generic_arguments.size()) {
                return nullptr;
            }

            return type_resolver.resolve_type(type.generic_arguments[index].type);
        }

        return nullptr;
    }

    const StructType* instantiate_facade(const PrimitiveFacadeInfo& info,
                                         const Type* backing_type) const {
        if (info.type == nullptr) {
            return nullptr;
        }

        if (!info.is_generic()) {
            return info.type;
        }

        CompileTimeEnvironment substitution_map;
        for (const auto& parameter : info.type->generic_parameters) {
            if (parameter.name == info.backing_parameter) {
                substitution_map.bind(parameter, TypeBinding(backing_type));
            }
        }

        return type_resolver.instantiate_struct(*info.type, substitution_map);
    }

    const Type* find_parameter_constraint(const StructType& type, const std::string& name) const {
        for (const auto& parameter : type.generic_parameters) {
            if (parameter.name == name) {
                return parameter.type;
            }
        }

        return nullptr;
    }

  public:
    FacadeResolver(SemaContext& sema, TypeResolver& type_resolver)
        : sema(sema), type_resolver(type_resolver) {}

    void register_struct(const StructType& type, Visibility::Type visibility) {
        if (visibility != Visibility::Type::Public) {
            return;
        }

        const Type* backing_type = nullptr;
        for (const auto* interface_type : type.interfaces) {
            if (interface_type == nullptr || interface_type->name != "Primitive" ||
                interface_type->generic_arguments.size() != 1) {
                continue;
            }

            backing_type = interface_type->generic_arguments.front().type;
            break;
        }

        const auto* value_field = type.find_field("value");
        if (backing_type == nullptr || value_field == nullptr || value_field->type == nullptr) {
            return;
        }

        const auto* left_named = backing_type->as<NamedType>();
        const auto* right_named = value_field->type->as<NamedType>();
        if (left_named != nullptr && right_named != nullptr &&
            left_named->name == right_named->name) {
            const auto* named = backing_type->as<NamedType>();
            auto info = PrimitiveFacadeInfo(type.name, &type, nullptr, named->name,
                                            find_parameter_constraint(type, named->name));
            sema.facades.register_facade(type.name, std::move(info));
            return;
        }

        const auto* resolved_backing = type_resolver.resolve_type(backing_type);
        const auto* resolved_value = type_resolver.resolve_type(value_field->type);
        if (resolved_backing == nullptr || resolved_value == nullptr ||
            !resolved_value->accepts(resolved_backing)) {
            return;
        }

        auto info = PrimitiveFacadeInfo(type.name, &type, resolved_backing, std::string(), nullptr);
        sema.facades.register_facade(type.name, std::move(info), resolved_backing);
    }

    const StructType* resolve_facade(const Type* type) const {
        const auto* resolved = type_resolver.resolve_type(type);
        if (resolved == nullptr) {
            return nullptr;
        }

        if (const auto* struct_type = resolved->as<StructType>(); struct_type != nullptr) {
            if (lookup_info_by_name(struct_type->name) != nullptr) {
                return struct_type;
            }
        }

        const auto* info = lookup_info_by_backing(resolved);
        if (info == nullptr) {
            return nullptr;
        }

        return instantiate_facade(*info, resolved);
    }

    const PrimitiveFacadeInfo* resolve_facade_info(const Type* type) const {
        const auto* resolved = type_resolver.resolve_type(type);
        if (resolved == nullptr) {
            return nullptr;
        }

        if (const auto* struct_type = resolved->as<StructType>(); struct_type != nullptr) {
            if (auto* info = lookup_info_by_name(struct_type->name); info != nullptr) {
                return info;
            }
        }

        return lookup_info_by_backing(resolved);
    }

    Scope* resolve_scope(const Type* type) const {
        const auto* facade_type = resolve_facade(type);
        if (facade_type == nullptr) {
            return nullptr;
        }

        return static_cast<Scope*>(facade_type->member_scope);
    }

    const Type* resolve_backing(const Type* type) const {
        const auto* resolved = type_resolver.resolve_type(type);
        const auto* struct_type = resolved != nullptr ? resolved->as<StructType>() : nullptr;
        if (struct_type == nullptr) {
            return nullptr;
        }

        const auto* info = lookup_info_by_name(struct_type->name);
        if (info == nullptr) {
            return nullptr;
        }

        if (info->is_generic()) {
            return resolve_generic_backing(*info, *struct_type);
        }

        return info->backing_type;
    }

    const Type* resolve_value(const Type* type) const {
        if (const auto* backing = resolve_backing(type); backing != nullptr) {
            return backing;
        }

        if (resolve_facade(type) != nullptr) {
            return type_resolver.resolve_type(type);
        }

        return nullptr;
    }

    bool accepts(const Type* expected_type, const Type* actual_type) const {
        const auto* expected = type_resolver.resolve_type(expected_type);
        const auto* actual = type_resolver.resolve_type(actual_type);

        if (expected == nullptr) {
            return false;
        }

        if (actual == nullptr) {
            return expected->is<PointerType>();
        }

        if (const auto* expected_pointer = expected->as<PointerType>();
            expected_pointer != nullptr && actual->is<StringType>()) {
            const auto* element = type_resolver.resolve_type(expected_pointer->element);
            return element == nullptr || element->is<VoidType>() || element->is<CharType>() ||
                   (element->is<IntegerType>() && element->as<IntegerType>()->is_unsigned &&
                    element->as<IntegerType>()->size == 8);
        }

        if (expected->is<StringType>()) {
            if (const auto* actual_pointer = actual->as<PointerType>(); actual_pointer != nullptr) {
                const auto* element = type_resolver.resolve_type(actual_pointer->element);
                return element == nullptr || element->is<VoidType>() || element->is<CharType>() ||
                       (element->is<IntegerType>() && element->as<IntegerType>()->is_unsigned &&
                        element->as<IntegerType>()->size == 8) ||
                       element->is<ArrayType>();
            }
        }

        if (expected->accepts(actual)) {
            return true;
        }

        if (const auto* expected_pointer = expected->as<PointerType>()) {
            if (const auto* backing = resolve_backing(expected_pointer->element);
                backing != nullptr) {
                if (accepts(backing, actual)) {
                    return true;
                }
                const auto* ptr_backing =
                    sema.types.create<PointerType>(backing, expected_pointer->is_mutable);
                if (ptr_backing->accepts(actual) || accepts(ptr_backing, actual)) {
                    return true;
                }
            }
        }

        if (const auto* actual_pointer = actual->as<PointerType>()) {
            if (const auto* backing = resolve_backing(actual_pointer->element);
                backing != nullptr) {
                if (accepts(expected, backing)) {
                    return true;
                }
                const auto* ptr_backing =
                    sema.types.create<PointerType>(backing, actual_pointer->is_mutable);
                if (expected->accepts(ptr_backing) || accepts(expected, ptr_backing)) {
                    return true;
                }
            }
        }

        if (const auto* backing = resolve_backing(expected); backing != nullptr) {
            expected = backing;
        }

        if (const auto* backing = resolve_backing(actual); backing != nullptr) {
            actual = backing;
        }

        return expected->accepts(actual);
    }

    Expression* extract_literal_value(StructLiteralExpression& node) const {
        if (resolve_backing(node.type) == nullptr) {
            return nullptr;
        }

        for (auto* field : node.fields) {
            if (field->name == "value") {
                return field->value;
            }
        }

        return nullptr;
    }
};
