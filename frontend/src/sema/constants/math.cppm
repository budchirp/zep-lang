module;

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

export module zep.frontend.sema.constants.math;

import zep.common.diagnostic.collection;
import zep.common.source.span;
import zep.frontend.kind;
import zep.frontend.sema.types;

export class IntegerUse {
  public:
    enum class Type : std::uint8_t { Literal, Arithmetic };
};

export class ConstantMath {
  private:
    class Range {
      public:
        __int128 minimum;
        __int128 maximum;

        explicit Range(const Type& type) {
            const auto character = type.is_char();
            const auto width = character ? 8 : type.bit_width();
            const auto is_unsigned = character || type.is_unsigned_type();

            minimum = is_unsigned ? __int128{0} : -(__int128{1} << (width - 1));
            maximum = (__int128{1} << (is_unsigned ? width : width - 1)) - 1;
        }
    };

    static __int128 number(const CompileTimeValue& value) {
        switch (value.kind) {
        case CompileTimeValue::Kind::Type::SignedInteger:
            return static_cast<__int128>(std::get<std::int64_t>(value.payload));
        case CompileTimeValue::Kind::Type::Char:
            return static_cast<__int128>(std::get<std::uint8_t>(value.payload));
        default:
            return static_cast<__int128>(std::get<std::uint64_t>(value.payload));
        }
    }

  public:
    static std::optional<CompileTimeValue> integer(
        Diagnostics& diagnostics, __int128 number, const Type* type, Span span,
        IntegerUse::Type use = IntegerUse::Type::Literal) {
        if (type == nullptr || (!type->is_integer() && !type->is_char())) {
            diagnostics.add_error(span, "constant expression requires an integer type");

            return std::nullopt;
        }

        Range range(*type);

        if (number < range.minimum || number > range.maximum) {
            diagnostics.add_error(span, use == IntegerUse::Type::Arithmetic
                ? "integer overflow in constant expression"
                : "constant integer is out of range for '" + type->to_string() + "'");

            return std::nullopt;
        }

        if (type->is_char()) {
            return CompileTimeValue(CompileTimeValue::Kind::Type::Char, type,
                                    static_cast<std::uint8_t>(number));
        }

        if (type->is_unsigned_type()) {
            return CompileTimeValue(CompileTimeValue::Kind::Type::UnsignedInteger, type,
                                    static_cast<std::uint64_t>(number));
        }

        return CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, type,
                                static_cast<std::int64_t>(number));
    }

    static std::optional<CompileTimeValue> binary(
        Diagnostics& diagnostics, BinaryOperator::Type operation,
        const CompileTimeValue& left, const CompileTimeValue& right, const Type* type, Span span) {
        if (!left.is_integer() || left.kind != right.kind) {
            diagnostics.add_error(span, "constant expression requires matching integer operands");

            return std::nullopt;
        }

        const auto first = number(left);
        const auto second = number(right);
        auto result = __int128{0};
        auto overflow = false;

        switch (operation) {
        case BinaryOperator::Type::Plus:
            overflow = __builtin_add_overflow(first, second, &result);
            break;
        case BinaryOperator::Type::Minus:
            overflow = __builtin_sub_overflow(first, second, &result);
            break;
        case BinaryOperator::Type::Asterisk:
            overflow = __builtin_mul_overflow(first, second, &result);
            break;
        case BinaryOperator::Type::Divide:
        case BinaryOperator::Type::Modulo:
            if (second == 0) {
                diagnostics.add_error(span, operation == BinaryOperator::Type::Divide
                    ? "division by zero in constant expression"
                    : "modulo by zero in constant expression");

                return std::nullopt;
            }

            if (left.kind == CompileTimeValue::Kind::Type::SignedInteger &&
                left.type != nullptr && second == -1 && first == Range(*left.type).minimum) {
                overflow = true;
                break;
            }

            result = operation == BinaryOperator::Type::Divide ? first / second : first % second;
            break;
        case BinaryOperator::Type::LessThan:
            return CompileTimeValue(CompileTimeValue::Kind::Type::Boolean, type, first < second);
        case BinaryOperator::Type::LessEqual:
            return CompileTimeValue(CompileTimeValue::Kind::Type::Boolean, type, first <= second);
        case BinaryOperator::Type::GreaterThan:
            return CompileTimeValue(CompileTimeValue::Kind::Type::Boolean, type, first > second);
        case BinaryOperator::Type::GreaterEqual:
            return CompileTimeValue(CompileTimeValue::Kind::Type::Boolean, type, first >= second);
        default:
            diagnostics.add_error(span, "unsupported integer operator in constant expression");

            return std::nullopt;
        }

        if (overflow) {
            diagnostics.add_error(span, "integer overflow in constant expression");

            return std::nullopt;
        }

        return integer(diagnostics, result, type, span, IntegerUse::Type::Arithmetic);
    }

    static std::optional<CompileTimeValue> cast(
        Diagnostics& diagnostics, const CompileTimeValue& value, const Type* type, Span span) {
        if (type == nullptr || !type->is_scalar()) {
            diagnostics.add_error(span, "unsupported const cast target");

            return std::nullopt;
        }

        if (type->is_boolean()) {
            if (value.kind != CompileTimeValue::Kind::Type::Boolean) {
                diagnostics.add_error(span, "cannot cast const value to boolean");

                return std::nullopt;
            }
        } else if (type->is_integer() || type->is_char()) {
            if (!value.is_integer() && value.kind != CompileTimeValue::Kind::Type::Char) {
                diagnostics.add_error(span, "cannot cast const value to integer");

                return std::nullopt;
            }

            return integer(diagnostics, number(value), type, span);
        } else if (!type->is<StringType>() || value.kind != CompileTimeValue::Kind::Type::String) {
            diagnostics.add_error(span, "unsupported const cast");

            return std::nullopt;
        }

        auto result = value;
        result.type = type;

        return result;
    }
};
