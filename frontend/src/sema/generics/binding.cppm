module;

#include <functional>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

export module zep.frontend.sema.types:binding;

export import :value;
import :base;
import zep.common.hash;

export class TypeBinding {
  public:
    const Type* type;

    explicit TypeBinding(const Type* type) : type(type) {}

    bool operator==(const TypeBinding& other) const {
        return type == other.type ||
               (type != nullptr && other.type != nullptr && type->same(other.type));
    }
};

export class ConstBinding {
  private:
    ConstBinding(const Type* type, void* expression, const void* declaration)
        : type(type), value(std::nullopt), expression(expression), declaration(declaration) {}

  public:
    const Type* type;
    std::optional<CompileTimeValue> value;
    void* expression;
    const void* declaration;

    explicit ConstBinding(CompileTimeValue value)
        : type(value.type), value(std::move(value)), expression(nullptr), declaration(nullptr) {}

    static ConstBinding dependent_expression(void* expression, const Type* type) {
        return ConstBinding(type, expression, nullptr);
    }

    static ConstBinding dependent_parameter(const void* declaration, const Type* type) {
        return ConstBinding(type, nullptr, declaration);
    }

    bool is_concrete() const {
        return value.has_value();
    }

    bool operator==(const ConstBinding& other) const {
        return (type == other.type || (type != nullptr && other.type != nullptr &&
                                       type->same(other.type))) &&
               value == other.value && expression == other.expression &&
               declaration == other.declaration;
    }
};

export using GenericBinding = std::variant<TypeBinding, ConstBinding>;

export class GenericBindingHash {
  public:
    std::size_t operator()(const GenericBinding& binding) const {
        return std::visit(
            [](const auto& value) -> std::size_t {
                using Value = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<Value, TypeBinding>) {
                    return value.type == nullptr ? 0 : value.type->hash();
                } else {
                    auto result = value.type == nullptr ? 0 : value.type->hash();

                    if (value.value.has_value()) {
                        result = Hash::combine(result, value.value->hash());
                    }

                    result = Hash::combine(result, std::hash<void*>{}(value.expression));

                    return Hash::combine(result, std::hash<const void*>{}(value.declaration));
                }
            },
            binding);
    }
};
