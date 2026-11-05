module;

export module zep.frontend.sema.types.compatibility;

import zep.frontend.sema.types;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.resolution.facades;

export class Compatibility {
  private:
    TypeArena& types;
    TypeResolver& resolver;
    const Facades& facades;

    bool string_pointer(const PointerType& pointer) const {
        const auto* element = resolver.resolve_type(pointer.element);

        if (element == nullptr || element->is<VoidType>() || element->is<CharType>()) {
            return true;
        }

        const auto* integer = element->as<IntegerType>();

        return integer != nullptr && integer->is_unsigned && integer->size == 8;
    }

  public:
    Compatibility(TypeArena& types, TypeResolver& resolver, const Facades& facades)
        : types(types), resolver(resolver), facades(facades) {}

    bool accepts(const Type* expected, const Type* actual) const {
        expected = resolver.resolve_type(expected);
        actual = resolver.resolve_type(actual);

        if (expected == nullptr) {
            return false;
        }

        if (actual == nullptr) {
            return expected->is<PointerType>();
        }

        if (const auto* pointer = expected->as<PointerType>();
            pointer != nullptr && actual->is<StringType>() && string_pointer(*pointer)) {
            return true;
        }

        if (const auto* pointer = actual->as<PointerType>();
            pointer != nullptr && expected->is<StringType>()) {
            const auto* element = resolver.resolve_type(pointer->element);

            if (string_pointer(*pointer) || (element != nullptr && element->is<ArrayType>())) {
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
