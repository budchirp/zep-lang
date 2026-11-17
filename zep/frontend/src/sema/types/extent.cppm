module;

#include <cstddef>
#include <variant>

export module zep.frontend.sema.types:extent;

export class UnsizedArrayExtent {
  public:
    UnsizedArrayExtent() = default;

    bool operator==(const UnsizedArrayExtent&) const = default;
};

export class ConcreteArrayExtent {
  public:
    std::size_t value;

    explicit ConcreteArrayExtent(std::size_t value) : value(value) {}

    bool operator==(const ConcreteArrayExtent&) const = default;
};

export class DependentArrayExtent {
  public:
    void* expression;
    const void* declaration;

    explicit DependentArrayExtent(void* expression, const void* declaration = nullptr)
        : expression(expression), declaration(declaration) {}

    bool operator==(const DependentArrayExtent& other) const {
        if (declaration != nullptr && other.declaration != nullptr) {
            return declaration == other.declaration;
        }

        return expression == other.expression;
    }
};

export using ArrayExtent =
    std::variant<UnsizedArrayExtent, ConcreteArrayExtent, DependentArrayExtent>;
