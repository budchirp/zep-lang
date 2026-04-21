module;

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.sema.type;

import zep.frontend.sema.type.type_id;

export class Type {
  private:
  public:
    class Kind {
      private:
      public:
        enum class Type : std::uint8_t {
            Any,
            Unknown,
            Void,
            String,
            Boolean,
            Integer,
            Float,
            Named,
            Array,
            Pointer,
            Struct,
            Function
        };
    };

  protected:
    explicit Type(Kind::Type kind, TypeId id) : kind(kind), id(id) {}

  public:
    const Kind::Type kind;

    const TypeId id;

    Type(const Type&) = delete;
    Type& operator=(const Type&) = delete;
    Type(Type&&) = delete;
    Type& operator=(Type&&) = delete;

    virtual ~Type() = default;

    bool is_numeric() const {
        return kind == Kind::Type::Integer || kind == Kind::Type::Float ||
               kind == Kind::Type::Unknown;
    }

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
};

export class AnyType : public Type {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Any;

    explicit AnyType(TypeId id) : Type(static_kind, id) {}
};

export class UnknownType : public Type {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Unknown;

    explicit UnknownType(TypeId id) : Type(static_kind, id) {}
};

export class VoidType : public Type {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Void;

    explicit VoidType(TypeId id) : Type(static_kind, id) {}
};

export class StringType : public Type {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::String;

    explicit StringType(TypeId id) : Type(static_kind, id) {}
};

export class BooleanType : public Type {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Boolean;

    explicit BooleanType(TypeId id) : Type(static_kind, id) {}
};

export class IntegerType : public Type {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Integer;

    bool is_unsigned;
    std::uint8_t size;

    IntegerType(TypeId id, bool is_unsigned, std::uint8_t size)
        : Type(static_kind, id), is_unsigned(is_unsigned), size(size) {}
};

export class FloatType : public Type {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Float;

    std::uint8_t size;

    FloatType(TypeId id, std::uint8_t size) : Type(static_kind, id), size(size) {}
};

export class GenericParameterType {
  private:
  public:
    std::string name;
    TypeId constraint;

    GenericParameterType(std::string name, TypeId constraint)
        : name(std::move(name)), constraint(constraint) {}
};

export class GenericArgumentType {
  private:
  public:
    std::string name;
    TypeId type;

    GenericArgumentType(std::string name, TypeId type) : name(std::move(name)), type(type) {}
};

export class ParameterType {
  private:
  public:
    std::string name;
    TypeId type;

    ParameterType(std::string name, TypeId type) : name(std::move(name)), type(type) {}
};

export class StructFieldType {
  private:
  public:
    std::string name;
    TypeId type;

    StructFieldType(std::string name, TypeId type) : name(std::move(name)), type(type) {}
};

export class NamedType : public Type {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Named;

    std::string name;
    std::vector<GenericArgumentType> generic_arguments;

    NamedType(TypeId id, std::string name, std::vector<GenericArgumentType> generic_arguments)
        : Type(static_kind, id), name(std::move(name)),
          generic_arguments(std::move(generic_arguments)) {}
};

export class ArrayType : public Type {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Array;

    TypeId element;
    std::optional<std::size_t> size;

    ArrayType(TypeId id, TypeId element, std::optional<std::size_t> size)
        : Type(static_kind, id), element(element), size(size) {}
};

export class PointerType : public Type {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Pointer;

    TypeId element;
    bool is_mutable;

    PointerType(TypeId id, TypeId element, bool is_mutable)
        : Type(static_kind, id), element(element), is_mutable(is_mutable) {}
};

export class StructType : public Type {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Struct;

    std::string name;
    std::vector<GenericParameterType> generic_parameters;
    std::vector<StructFieldType> fields;

    StructType(TypeId id, std::string name, std::vector<GenericParameterType> generic_parameters,
               std::vector<StructFieldType> fields)
        : Type(static_kind, id), name(std::move(name)),
          generic_parameters(std::move(generic_parameters)), fields(std::move(fields)) {}
};

export class FunctionType : public Type {
  private:
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Function;

    std::string name;
    TypeId return_type;
    std::vector<ParameterType> parameters;
    std::vector<GenericParameterType> generic_parameters;
    bool variadic;

    FunctionType(TypeId id, std::string name, TypeId return_type,
                 std::vector<ParameterType> parameters,
                 std::vector<GenericParameterType> generic_parameters, bool variadic)
        : Type(static_kind, id), name(std::move(name)), return_type(return_type),
          parameters(std::move(parameters)), generic_parameters(std::move(generic_parameters)),
          variadic(variadic) {}
};
