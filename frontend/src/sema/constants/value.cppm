module;

#include <bit>
#include <charconv>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module zep.frontend.sema.types:value;

import zep.common.hash;
import :base;

export class CompileTimeValue {
  private:
    static std::string encode_string(const std::string& value) {
        auto result = std::to_string(value.size()) + "_";

        for (const auto character : value) {
            result += std::to_string(
                          static_cast<unsigned>(static_cast<unsigned char>(character))) +
                      "_";
        }

        return result;
    }

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

    bool is_integer() const {
        return kind == Kind::Type::SignedInteger || kind == Kind::Type::UnsignedInteger;
    }

    bool operator==(const CompileTimeValue& other) const {
        return kind == other.kind &&
               (type == other.type || (type != nullptr && type->same(other.type))) &&
               payload == other.payload;
    }

    std::size_t hash() const {
        auto result = Hash::combine(static_cast<std::size_t>(kind), Type::hash(type));

        return std::visit(
            [result](const auto& value) mutable {
                using Value = std::decay_t<decltype(value)>;

                if constexpr (std::is_same_v<Value, std::vector<CompileTimeValue>>) {
                    for (const auto& element : value) {
                        result = Hash::combine(result, element.hash());
                    }
                } else {
                    result = Hash::combine(result, std::hash<Value>{}(value));
                }

                return result;
            },
            payload);
    }

    std::string stable_encoding() const {
        switch (kind) {
        case Kind::Type::SignedInteger: {
            auto value = std::to_string(std::get<std::int64_t>(payload));
            if (!value.empty() && value.front() == '-') {
                value.front() = 'n';
            } else {
                value.insert(value.begin(), 'p');
            }
            return "s" + value;
        }
        case Kind::Type::UnsignedInteger:
            return "u" + std::to_string(std::get<std::uint64_t>(payload));
        case Kind::Type::Float:
            return "f" + std::to_string(std::get<std::uint64_t>(payload));
        case Kind::Type::Boolean:
            return std::get<bool>(payload) ? "b1" : "b0";
        case Kind::Type::Char:
            return "h" + std::to_string(std::get<std::uint8_t>(payload));
        case Kind::Type::String:
            return "t" + encode_string(std::get<std::string>(payload));
        case Kind::Type::Null:
            return "null";
        case Kind::Type::Struct:
        case Kind::Type::Array: {
            const auto& elements = std::get<std::vector<CompileTimeValue>>(payload);
            auto result = kind == Kind::Type::Struct ? std::string("s") : std::string("a");
            result += std::to_string(elements.size()) + "_";

            for (const auto& element : elements) {
                const auto encoded = element.stable_encoding();
                result += std::to_string(encoded.size()) + "_" + encoded;
            }

            return result;
        }
        }

        return {};
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

