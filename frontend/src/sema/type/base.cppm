module;

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.sema.type:base;

import zep.common.arena;

export class FunctionType;
export class FieldType;
export class NominalType;
export class StructType;
export class EnumType;
export class InterfaceType;

export class Type {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t {
            Any,
            Void,
            Never,
            String,
            Boolean,
            Char,
            Integer,
            Float,
            Named,
            Array,
            Pointer,
            Struct,
            Enum,
            Interface,
            Function,
        };
    };

  protected:
    Type(Kind::Type kind, std::string label) : kind(kind), label(std::move(label)) {}

  public:
    const Kind::Type kind;
    const std::string label;

    virtual ~Type() = default;

    template <typename T>
    T* as() {
        if (kind == T::static_kind) {
            return static_cast<T*>(this);
        }

        return nullptr;
    }

    template <typename T>
    const T* as() const {
        if (kind == T::static_kind) {
            return static_cast<const T*>(this);
        }

        return nullptr;
    }

    template <typename T>
    bool is() const {
        return static_cast<bool>(kind == T::static_kind);
    }

    bool is_numeric() const { return kind == Kind::Type::Integer || kind == Kind::Type::Float; }
    bool is_integer() const { return kind == Kind::Type::Integer; }
    bool is_char() const { return kind == Kind::Type::Char; }
    bool is_boolean() const { return kind == Kind::Type::Boolean; }

    virtual bool is_unsigned_type() const { return false; }
    virtual std::uint8_t bit_width() const { return static_cast<std::uint8_t>(byte_size() * 8); }

    bool is_scalar() const;

    virtual bool is_copy() const { return is_scalar(); }

    static std::size_t align_to(std::size_t value, std::size_t alignment) {
        if (alignment <= 1) {
            return value;
        }

        auto remainder = value % alignment;
        return remainder == 0 ? value : value + alignment - remainder;
    }

    const NominalType* as_nominal() const;

    static bool fields_accept(const std::vector<FieldType>& left,
                              const std::vector<FieldType>& right);

    static bool same(const Type* left, const Type* right) {
        return left == right || (left != nullptr && left->same(right));
    }

    static bool accepts(const Type* target, const Type* source) {
        return target != nullptr && target->accepts(source);
    }

    static std::size_t hash(const Type* type) { return type == nullptr ? 0 : type->hash(); }

    virtual std::size_t hash() const;

    bool accepts(const Type* other) const;

    bool same(const Type* other) const;

    virtual std::size_t byte_size() const = 0;
    virtual std::size_t alignment() const = 0;

    virtual std::string to_string() const = 0;
};

export using TypeArena = Arena<Type>;
