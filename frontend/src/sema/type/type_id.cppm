module;

#include <cstdint>

export module zep.frontend.sema.type.type_id;

export class TypeId {
  private:
    std::uint32_t value;

  public:
    constexpr TypeId() : value(0) {}

    explicit constexpr TypeId(std::uint32_t value) : value(value) {}

    constexpr std::uint32_t raw() const { return value; }

    constexpr bool is_valid() const { return value != 0; }

    constexpr bool operator==(const TypeId& other) const { return value == other.value; }

    constexpr bool operator!=(const TypeId& other) const { return value != other.value; }
};
