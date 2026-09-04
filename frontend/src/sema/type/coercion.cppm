module;

export module zep.frontend.sema.type.coercion;

import zep.frontend.node;
import zep.frontend.sema.context;
import zep.frontend.sema.type;
import zep.frontend.sema.kind;
import zep.frontend.sema.type.resolver;

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

        const auto coercion = classify(resolver, target, value->type);
        if (coercion == Coercion::Type::None) {
            return value;
        }

        return sema.nodes.create<CoerceExpression>(value->span, value, target, coercion,
                                                   value->type);
    }
};
