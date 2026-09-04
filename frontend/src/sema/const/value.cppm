module;

#include <bit>
#include <charconv>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module zep.frontend.sema.type:value;

import zep.common.diagnostic.diagnostic;
import zep.common.diagnostic.collection;
import zep.common.source.span;
import zep.frontend.sema.kind;
import :base;

export class CompileTimeValue;

export class CompileTimeValue {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t {
            SignedInteger,
            UnsignedInteger,
            Float,
            Boolean,
            Char,
            String,
            Null,
            Struct,
            Array,
        };
    };

    using Payload = std::variant<std::int64_t, std::uint64_t, bool, std::uint8_t, std::string,
                                 std::vector<CompileTimeValue>>;

    Kind::Type kind;
    const Type* type;
    Payload payload;

    CompileTimeValue(Kind::Type kind, const Type* type, Payload payload = std::int64_t{0})
        : kind(kind), type(type), payload(std::move(payload)) {}

    CompileTimeValue(double value, const Type* type)
        : kind(Kind::Type::Float), type(type),
          payload(type != nullptr && type->byte_size() == 4
                      ? static_cast<std::uint64_t>(
                            std::bit_cast<std::uint32_t>(static_cast<float>(value)))
                      : std::bit_cast<std::uint64_t>(value)) {}

    static bool integer_in_range(__int128 value, const Type* type) {
        if (type == nullptr || (!type->is_integer() && !type->is_char())) {
            return false;
        }

        const auto is_ch = type->is_char();
        const auto width = is_ch ? 8 : type->bit_width();
        const auto is_unsigned = is_ch || type->is_unsigned_type();
        const auto minimum = is_unsigned ? __int128{0} : -(__int128{1} << (width - 1));
        const auto maximum = (__int128{1} << (is_unsigned ? width : width - 1)) - 1;

        return value >= minimum && value <= maximum;
    }

    static std::optional<CompileTimeValue> checked_integer(Diagnostics& diagnostics, __int128 value,
                                                           const Type* type, Span span,
                                                           bool arithmetic = false) {
        const auto is_int = type != nullptr && type->is_integer();
        const auto is_ch = type != nullptr && type->is_char();
        if (!is_int && !is_ch) {
            diagnostics.add_error(span, "constant expression requires an integer type");
            return std::nullopt;
        }

        const auto width = is_ch ? 8 : type->bit_width();
        const auto is_unsigned = is_ch || type->is_unsigned_type();
        const auto minimum = is_unsigned ? __int128{0} : -(__int128{1} << (width - 1));
        const auto maximum = (__int128{1} << (is_unsigned ? width : width - 1)) - 1;
        if (value < minimum || value > maximum) {
            diagnostics.add_error(span, arithmetic ? "integer overflow in constant expression"
                                                   : "constant integer is out of range for '" +
                                                         type->to_string() + "'");
            return std::nullopt;
        }

        if (is_ch) {
            return CompileTimeValue(Kind::Type::Char, type, static_cast<std::uint8_t>(value));
        }

        return is_unsigned ? CompileTimeValue(Kind::Type::UnsignedInteger, type,
                                              static_cast<std::uint64_t>(value))
                           : CompileTimeValue(Kind::Type::SignedInteger, type,
                                              static_cast<std::int64_t>(value));
    }

    template <typename Number>
    static std::optional<CompileTimeValue>
    evaluate_binary_impl(Diagnostics& diagnostics, BinaryOperator::Type operation, Number left,
                         Number right, const Type* type, Span span) {
        using Operator = BinaryOperator::Type;
        auto result = Number{0};
        auto overflow = false;

        switch (operation) {
        case Operator::Plus:
            overflow = __builtin_add_overflow(left, right, &result);
            break;
        case Operator::Minus:
            overflow = __builtin_sub_overflow(left, right, &result);
            break;
        case Operator::Asterisk:
            overflow = __builtin_mul_overflow(left, right, &result);
            break;
        case Operator::Divide:
        case Operator::Modulo:
            if (right == 0) {
                diagnostics.add_error(span, operation == Operator::Divide
                                                ? "division by zero in constant expression"
                                                : "modulo by zero in constant expression");
                return std::nullopt;
            }

            if constexpr (std::is_signed_v<Number>) {
                const auto minimum =
                    type != nullptr ? -(__int128{1} << (type->bit_width() - 1))
                                    : static_cast<__int128>(std::numeric_limits<Number>::min());
                if (static_cast<__int128>(left) == minimum && right == -1) {
                    diagnostics.add_error(span, "integer overflow in constant expression");
                    return std::nullopt;
                }
            }

            result = operation == Operator::Divide ? left / right : left % right;
            break;
        case Operator::LessThan:
            return CompileTimeValue(Kind::Type::Boolean, type, left < right);
        case Operator::LessEqual:
            return CompileTimeValue(Kind::Type::Boolean, type, left <= right);
        case Operator::GreaterThan:
            return CompileTimeValue(Kind::Type::Boolean, type, left > right);
        case Operator::GreaterEqual:
            return CompileTimeValue(Kind::Type::Boolean, type, left >= right);
        default:
            diagnostics.add_error(span, "unsupported integer operator in constant expression");
            return std::nullopt;
        }

        if (overflow) {
            diagnostics.add_error(span, "integer overflow in constant expression");
            return std::nullopt;
        }

        return checked_integer(diagnostics, static_cast<__int128>(result), type, span, true);
    }

    static std::optional<CompileTimeValue> evaluate_binary(Diagnostics& diagnostics,
                                                           BinaryOperator::Type operation,
                                                           std::int64_t left, std::int64_t right,
                                                           const Type* type, Span span) {
        return evaluate_binary_impl(diagnostics, operation, left, right, type, span);
    }

    static std::optional<CompileTimeValue> evaluate_binary(Diagnostics& diagnostics,
                                                           BinaryOperator::Type operation,
                                                           std::uint64_t left, std::uint64_t right,
                                                           const Type* type, Span span) {
        return evaluate_binary_impl(diagnostics, operation, left, right, type, span);
    }

    std::optional<CompileTimeValue> cast_to(Diagnostics& diagnostics, const Type* target_type,
                                            Span span) const {
        if (target_type == nullptr || !target_type->is_scalar()) {
            diagnostics.add_error(span, "unsupported const cast target");
            return std::nullopt;
        }

        if (target_type->is_boolean()) {
            if (kind == Kind::Type::Boolean) {
                auto copy = *this;
                copy.type = target_type;
                return copy;
            }
            diagnostics.add_error(span, "cannot cast const value to boolean");
            return std::nullopt;
        }

        if (target_type->is_integer() || target_type->is_char()) {
            if (!is_integer() && kind != Kind::Type::Char) {
                diagnostics.add_error(span, "cannot cast const value to integer");
                return std::nullopt;
            }

            if (kind == Kind::Type::Char) {
                return checked_integer(diagnostics,
                                       static_cast<__int128>(std::get<std::uint8_t>(payload)),
                                       target_type, span);
            }

            const auto numeric = kind == Kind::Type::SignedInteger
                                     ? static_cast<__int128>(std::get<std::int64_t>(payload))
                                     : static_cast<__int128>(std::get<std::uint64_t>(payload));
            return checked_integer(diagnostics, numeric, target_type, span);
        }

        if (target_type->kind == Type::Kind::Type::String && kind == Kind::Type::String) {
            auto copy = *this;
            copy.type = target_type;
            return copy;
        }

        diagnostics.add_error(span, "unsupported const cast");
        return std::nullopt;
    }

    bool is_integer() const {
        return kind == Kind::Type::SignedInteger || kind == Kind::Type::UnsignedInteger;
    }

    bool operator==(const CompileTimeValue& other) const {
        return kind == other.kind &&
               (type == other.type || (type != nullptr && type->same(other.type))) &&
               payload == other.payload;
    }

    std::size_t hash() const {
        return std::hash<std::string>{}(to_string()) ^
               (type == nullptr ? 0 : std::hash<std::string>{}(type->to_string()));
    }

    std::optional<std::int64_t> try_as_signed_integer() const {
        switch (kind) {
        case Kind::Type::SignedInteger:
            return std::get<std::int64_t>(payload);
        case Kind::Type::UnsignedInteger: {
            const auto value = std::get<std::uint64_t>(payload);
            if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                return std::nullopt;
            }

            return static_cast<std::int64_t>(value);
        }
        case Kind::Type::Char:
            return static_cast<std::int64_t>(std::get<std::uint8_t>(payload));
        case Kind::Type::Boolean:
        case Kind::Type::Float:
        case Kind::Type::String:
        case Kind::Type::Null:
        case Kind::Type::Struct:
        case Kind::Type::Array:
            return std::nullopt;
        }

        return std::nullopt;
    }

    std::optional<std::uint64_t> try_as_unsigned_integer() const {
        switch (kind) {
        case Kind::Type::SignedInteger: {
            const auto value = std::get<std::int64_t>(payload);
            if (value < 0) {
                return std::nullopt;
            }

            return static_cast<std::uint64_t>(value);
        }
        case Kind::Type::UnsignedInteger:
            return std::get<std::uint64_t>(payload);
        case Kind::Type::Char:
            return static_cast<std::uint64_t>(std::get<std::uint8_t>(payload));
        case Kind::Type::Boolean:
        case Kind::Type::Float:
        case Kind::Type::String:
        case Kind::Type::Null:
        case Kind::Type::Struct:
        case Kind::Type::Array:
            return std::nullopt;
        }

        return std::nullopt;
    }

    std::int64_t as_signed_integer() const {
        auto value = try_as_signed_integer();
        if (!value.has_value()) {
            throw std::runtime_error("compile-time value is not a concrete signed integer");
        }
        return *value;
    }

    std::uint64_t as_unsigned_integer() const {
        auto value = try_as_unsigned_integer();
        if (!value.has_value()) {
            throw std::runtime_error("compile-time value is not a concrete unsigned integer");
        }
        return *value;
    }

    std::string to_string() const {
        switch (kind) {
        case Kind::Type::SignedInteger:
            return std::to_string(std::get<std::int64_t>(payload));
        case Kind::Type::UnsignedInteger:
            return std::to_string(std::get<std::uint64_t>(payload));
        case Kind::Type::Float: {
            const auto bits = std::get<std::uint64_t>(payload);
            const auto value =
                type != nullptr && type->byte_size() == 4
                    ? static_cast<double>(std::bit_cast<float>(static_cast<std::uint32_t>(bits)))
                    : std::bit_cast<double>(bits);
            char buffer[64];
            const auto result =
                std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general,
                              std::numeric_limits<double>::max_digits10);
            return std::string(buffer, result.ptr);
        }
        case Kind::Type::Boolean:
            return std::get<bool>(payload) ? "true" : "false";
        case Kind::Type::Char:
            return std::to_string(std::get<std::uint8_t>(payload));
        case Kind::Type::String:
            return "\"" + std::get<std::string>(payload) + "\"";
        case Kind::Type::Null:
            return "null";
        case Kind::Type::Struct: {
            const auto& fields = std::get<std::vector<CompileTimeValue>>(payload);
            auto result = std::string("{");
            for (std::size_t i = 0; i < fields.size(); ++i) {
                if (i > 0) {
                    result += ", ";
                }
                result += fields[i].to_string();
            }
            result += "}";
            return result;
        }
        case Kind::Type::Array: {
            const auto& elements = std::get<std::vector<CompileTimeValue>>(payload);
            auto result = std::string("[");
            for (std::size_t i = 0; i < elements.size(); ++i) {
                if (i > 0) {
                    result += ", ";
                }
                result += elements[i].to_string();
            }
            result += "]";
            return result;
        }
        }
        return {};
    }
};

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
    const void* expression;
    const void* declaration;

    explicit DependentArrayExtent(const void* expression, const void* declaration = nullptr)
        : expression(expression), declaration(declaration) {}

    bool operator==(const DependentArrayExtent& other) const {
        return declaration != nullptr && other.declaration != nullptr
                   ? declaration == other.declaration
                   : expression == other.expression;
    }
};

export using ArrayExtent =
    std::variant<UnsizedArrayExtent, ConcreteArrayExtent, DependentArrayExtent>;

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
  public:
    const Type* type;
    std::optional<CompileTimeValue> value;
    const void* source;
    bool source_is_expression;

    explicit ConstBinding(CompileTimeValue value)
        : type(value.type), value(std::move(value)), source(nullptr), source_is_expression(false) {}

    ConstBinding(const void* source, const Type* type, bool source_is_expression)
        : type(type), value(std::nullopt), source(source),
          source_is_expression(source_is_expression) {}

    bool is_concrete() const { return value.has_value(); }
    std::string to_string() const { return value.has_value() ? value->to_string() : "<dependent>"; }

    bool operator==(const ConstBinding& other) const {
        return (type == other.type || (type != nullptr && other.type != nullptr &&
                                       type->to_string() == other.type->to_string())) &&
               value == other.value && source == other.source &&
               source_is_expression == other.source_is_expression;
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
                        result ^= value.value->hash() + 0x9e3779b9 + (result << 6) + (result >> 2);
                    }
                    result ^= std::hash<const void*>{}(value.source) + 0x9e3779b9 + (result << 6) +
                              (result >> 2);
                    result ^= static_cast<std::size_t>(value.source_is_expression);
                    return result;
                }
            },
            binding);
    }
};
