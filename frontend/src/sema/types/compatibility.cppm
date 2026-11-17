module;

export module zep.frontend.sema.types.compatibility;

import zep.frontend.sema.types;
import zep.frontend.kind;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.resolution.facades;

export class Compatibility {
  private:
    TypeArena& types;
    TypeResolver& resolver;
    const Facades& facades;

    bool string_element(const Type* type) const {
        const auto* element = resolver.resolve_type(type);

        if (element == nullptr || element->is<VoidType>() || element->is<CharType>()) {
            return true;
        }

        const auto* integer = element->as<IntegerType>();

        return integer != nullptr && integer->is_unsigned && integer->size == 8;
    }

  public:
    Compatibility(TypeArena& types, TypeResolver& resolver, const Facades& facades)
        : types(types), resolver(resolver), facades(facades) {}

    static Coercion::Type coercion(const Type* target, const Type* source) {
        if (target == nullptr || source == nullptr) {
            return Coercion::Type::None;
        }

        const auto* interface = target->as<InterfaceType>();
        const auto* pointer = source->as<PointerType>();
        const auto* structure = pointer != nullptr
            ? pointer->element->as<StructType>() : source->as<StructType>();

        if (interface != nullptr && structure != nullptr && structure->implements(interface)) {
            return Coercion::Type::InterfaceValue;
        }

        const auto* base = target->as<StructType>();
        const auto* derived = source->as<StructType>();

        if (base != nullptr && derived != nullptr && derived->inherits_from(base)) {
            return Coercion::Type::BaseSlice;
        }

        return Coercion::Type::None;
    }

    bool accepts(const Type* expected, const Type* actual) const {
        expected = resolver.resolve_type(expected);
        actual = resolver.resolve_type(actual);

        if (expected != nullptr && actual != nullptr && expected->accepts(actual)) {
            return true;
        }

        expected = resolver.constraint_type(expected);
        actual = resolver.constraint_type(actual);

        if (expected == nullptr) {
            return false;
        }

        if (actual == nullptr) {
            return expected->is<PointerType>();
        }

        if (const auto* pointer = expected->as<PointerType>();
            pointer != nullptr && actual->is<StringType>() && string_element(pointer->element)) {
            return true;
        }

        if (const auto* pointer = actual->as<PointerType>();
            pointer != nullptr && expected->is<StringType>()) {
            auto* element = resolver.resolve_type(pointer->element);

            if (const auto* array = element != nullptr ? element->as<ArrayType>() : nullptr;
                array != nullptr) {
                element = resolver.resolve_type(array->element);
            }

            if (string_element(element)) {
                return true;
            }
        }

        if (expected->accepts(actual)) {
            return true;
        }

        if (const auto* pointer = expected->as<PointerType>(); pointer != nullptr) {
            if (const auto* backing = facades.backing(pointer->element); backing != nullptr) {
                const auto* adapted = types.create<PointerType>(
                    backing, pointer->is_mutable, pointer->reference_kind);

                if (accepts(backing, actual) || accepts(adapted, actual)) {
                    return true;
                }
            }
        }

        if (const auto* pointer = actual->as<PointerType>(); pointer != nullptr) {
            if (const auto* backing = facades.backing(pointer->element); backing != nullptr) {
                const auto* adapted = types.create<PointerType>(
                    backing, pointer->is_mutable, pointer->reference_kind);

                if (accepts(expected, backing) || accepts(expected, adapted)) {
                    return true;
                }
            }
        }

        if (const auto* backing = facades.backing(expected); backing != nullptr) {
            expected = backing;
        }

        if (const auto* backing = facades.backing(actual); backing != nullptr) {
            actual = backing;
        }

        return expected->accepts(actual);
    }
};
