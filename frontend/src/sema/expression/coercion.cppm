module;

#include <cstddef>

export module zep.frontend.sema.expression.coercion;

import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.types;
import zep.frontend.kind;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.scopes.scope;
import zep.frontend.sema.symbols;

export class TypeCoercion {
  public:
    static Coercion::Type classify(TypeResolver& resolver, const Type* target, const Type* source) {
        const auto* resolved_target = resolver.resolve_type(target);
        const auto* resolved_source = resolver.resolve_type(source);

        if (resolved_target == nullptr || resolved_source == nullptr) {
            return Coercion::Type::None;
        }

        const auto* target_interface = resolved_target->as<InterfaceType>();
        const auto* source_struct_for_interface = resolved_source->as<StructType>();
        if (target_interface != nullptr && source_struct_for_interface != nullptr &&
            source_struct_for_interface->implements(target_interface)) {
            return Coercion::Type::InterfaceValue;
        }

        const auto* source_pointer_for_interface = resolved_source->as<PointerType>();
        const auto* source_pointer_struct =
            source_pointer_for_interface != nullptr
                ? source_pointer_for_interface->element->as<StructType>()
                : nullptr;
        if (target_interface != nullptr && source_pointer_struct != nullptr &&
            source_pointer_struct->implements(target_interface)) {
            return Coercion::Type::InterfaceValue;
        }

        const auto* target_struct = resolved_target->as<StructType>();
        const auto* source_struct_for_base = resolved_source->as<StructType>();
        if (target_struct != nullptr && source_struct_for_base != nullptr &&
            source_struct_for_base->inherits_from(target_struct)) {
            return Coercion::Type::BaseSlice;
        }

        return Coercion::Type::None;
    }

    static Expression* apply(SemaContext& sema, TypeResolver& resolver, Expression* value,
                             const Type* target) {
        if (value == nullptr || value->type == nullptr || target == nullptr) {
            return value;
        }

        const auto* resolved = resolver.resolve_type(target);
        if (const auto* reference = resolved != nullptr ? resolved->as<PointerType>() : nullptr;
            reference != nullptr && reference->reference_kind != PointerType::ReferenceKind::Type::None &&
            !Type::same(value->type, resolved)) {
            const auto* source = value->type->as<PointerType>();

            if (source != nullptr && source->reference_kind != PointerType::ReferenceKind::Type::None) {
                return value;
            }

            auto* address = sema.nodes.create<UnaryExpression>(
                value->span, reference->is_mutable ? UnaryOperator::Type::AddressOfMut : UnaryOperator::Type::AddressOf, value);
            address->type = resolved;
            return address;
        }

        const auto coercion = classify(resolver, target, value->type);
        if (coercion == Coercion::Type::None) {
            return value;
        }

        auto* result = sema.nodes.create<CoerceExpression>(value->span, value, target, coercion,
                                                         value->type);

        if (coercion == Coercion::Type::InterfaceValue) {
            const auto* source = resolver.resolve_type(value->type);

            if (const auto* pointer = source->as<PointerType>(); pointer != nullptr) {
                source = resolver.resolve_type(pointer->element);
            }

            const auto* structure = source->as<StructType>();
            const auto* interface = resolved->as<InterfaceType>();
            result->implementations.reserve(interface->methods.size());

            for (const auto& method : interface->methods) {
                for (const auto* owner = structure; owner != nullptr; owner = owner->base_type) {
                    const auto* members = Scope::members(owner);
                    const auto* overloads = members != nullptr
                        ? members->find_local_function_overloads(method.name) : nullptr;

                    if (overloads == nullptr) {
                        continue;
                    }

                    const auto* expected = resolver.method_type(method.type, structure);

                    for (const auto* function : *overloads) {
                        const auto* signature = resolver.method_type(function->signature(), owner);

                        if (expected->matches_method(*signature)) {
                            result->implementations.emplace_back(*function, signature);
                            break;
                        }
                    }

                    if (result->implementations.size() == method.index + 1) {
                        break;
                    }
                }
            }
        }

        return result;
    }
};
