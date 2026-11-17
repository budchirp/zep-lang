module;

#include <type_traits>

export module zep.common.tagged;

export class Tagged {
  private:
    template <typename Value, typename Owner>
    using Pointer = std::conditional_t<std::is_const_v<Owner>, const Value*, Value*>;

  public:
    template <typename Value, typename Owner>
    Pointer<Value, Owner> as(this Owner& owner) {
        if (owner.kind != Value::static_kind) {
            return nullptr;
        }

        return static_cast<Pointer<Value, Owner>>(&owner);
    }

    template <typename Value, typename Owner>
    bool is(this const Owner& owner) {
        return owner.kind == Value::static_kind;
    }
};
