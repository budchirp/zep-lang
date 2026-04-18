module;

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

export module zep.frontend.sema.type;

export class Type {
  public:
    class Kind {
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
    explicit Type(Kind::Type kind) : kind(kind) {}

    Type(const Type&) = delete;
    Type& operator=(const Type&) = delete;
    Type(Type&&) = default;
    Type& operator=(Type&&) = default;

  public:
    Kind::Type kind;

    virtual ~Type() = default;

    virtual std::string to_string() const = 0;

    bool is_numeric() const {
        return kind == Kind::Type::Integer || kind == Kind::Type::Float ||
               kind == Kind::Type::Unknown;
    }

    static bool compatible(const std::shared_ptr<Type>& left, const std::shared_ptr<Type>& right);

    bool operator==(const Type& other) const { return kind == other.kind; }

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
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Any;

    AnyType() : Type(static_kind) {}

    std::string to_string() const override { return "any"; }
};

export class UnknownType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Unknown;

    UnknownType() : Type(static_kind) {}

    std::string to_string() const override { return "unknown"; }
};

export class VoidType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Void;

    VoidType() : Type(static_kind) {}

    std::string to_string() const override { return "void"; }
};

export class StringType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::String;

    StringType() : Type(static_kind) {}

    std::string to_string() const override { return "string"; }
};

export class BooleanType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Boolean;

    BooleanType() : Type(static_kind) {}

    std::string to_string() const override { return "bool"; }
};

export class IntegerType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Integer;

    bool is_unsigned;
    std::uint8_t size;

    IntegerType(bool is_unsigned = false, std::uint8_t size = 32)
        : Type(static_kind), is_unsigned(is_unsigned), size(size) {}

    std::string to_string() const override {
        return (is_unsigned ? "u" : "i") + std::to_string(size);
    }
};

export class FloatType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Float;

    std::uint8_t size;

    explicit FloatType(std::uint8_t size = 64) : Type(static_kind), size(size) {}

    std::string to_string() const override { return "f" + std::to_string(size); }
};

export class GenericParameterType {
  public:
    std::string name;
    std::shared_ptr<Type> constraint;

    GenericParameterType(std::string name, std::shared_ptr<Type> constraint = nullptr)
        : name(std::move(name)), constraint(std::move(constraint)) {}

    std::string to_string() const {
        if (constraint != nullptr) {
            return name + ": " + constraint->to_string();
        }
        return name;
    }
};

export class GenericArgumentType {
  public:
    std::string name;
    std::shared_ptr<Type> type;

    GenericArgumentType(std::shared_ptr<Type> type, std::string name = "")
        : name(std::move(name)), type(std::move(type)) {}

    std::string to_string() const {
        if (!name.empty()) {
            return name + " = " + type->to_string();
        }
        return type->to_string();
    }
};

export class ParameterType {
  public:
    std::string name;
    std::shared_ptr<Type> type;

    ParameterType(std::string name, std::shared_ptr<Type> type)
        : name(std::move(name)), type(std::move(type)) {}

    std::string to_string() const { return name + ": " + type->to_string(); }
};

export class StructFieldType {
  public:
    std::string name;
    std::shared_ptr<Type> type;

    StructFieldType(std::string name, std::shared_ptr<Type> type)
        : name(std::move(name)), type(std::move(type)) {}

    std::string to_string() const { return name + ": " + type->to_string(); }
};

export class NamedType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Named;

    std::string name;
    std::vector<std::shared_ptr<GenericArgumentType>> generic_arguments;

    NamedType(std::string name,
              std::vector<std::shared_ptr<GenericArgumentType>> generic_arguments = {})
        : Type(static_kind), name(std::move(name)),
          generic_arguments(std::move(generic_arguments)) {}

    std::string to_string() const override { return name; }
};

export class ArrayType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Array;

    std::shared_ptr<Type> element;
    std::optional<std::size_t> size;

    ArrayType(std::shared_ptr<Type> element, std::optional<std::size_t> size = std::nullopt)
        : Type(static_kind), element(std::move(element)), size(size) {}

    std::string to_string() const override {
        return element->to_string() + "[" + (size.has_value() ? std::to_string(*size) : "") + "]";
    }
};

export class PointerType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Pointer;

    bool is_mutable;
    std::shared_ptr<Type> element;

    explicit PointerType(std::shared_ptr<Type> element, bool is_mutable = false)
        : Type(static_kind), is_mutable(is_mutable), element(std::move(element)) {}

    std::string to_string() const override {
        return is_mutable ? "*mut " + element->to_string() : "*" + element->to_string();
    }
};

export class StructType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Struct;

    std::string name;

    std::vector<std::shared_ptr<GenericParameterType>> generic_parameters;
    std::vector<std::shared_ptr<StructFieldType>> fields;

    StructType(std::string name,
               std::vector<std::shared_ptr<GenericParameterType>> generic_parameters,
               std::vector<std::shared_ptr<StructFieldType>> fields)
        : Type(static_kind), name(std::move(name)),
          generic_parameters(std::move(generic_parameters)), fields(std::move(fields)) {}

    std::string to_string() const override { return name; }
};

export class FunctionType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Function;

    std::string name;

    std::shared_ptr<Type> return_type;

    std::vector<std::shared_ptr<ParameterType>> parameters;
    std::vector<std::shared_ptr<GenericParameterType>> generic_parameters;

    bool variadic;

    FunctionType(std::string name, std::shared_ptr<Type> return_type,
                 std::vector<std::shared_ptr<ParameterType>> parameters,
                 std::vector<std::shared_ptr<GenericParameterType>> generic_parameters = {},
                 bool variadic = false)
        : Type(static_kind), name(std::move(name)), return_type(std::move(return_type)),
          parameters(std::move(parameters)), generic_parameters(std::move(generic_parameters)),
          variadic(variadic) {}

    bool conflicts_with(const FunctionType* other) const {
        if (other == nullptr) {
            return false;
        }

        if (parameters.size() != other->parameters.size()) {
            return false;
        }

        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (!Type::compatible(parameters[i]->type, other->parameters[i]->type)) {
                return false;
            }
        }

        return true;
    }

    std::string to_string() const override {
        return "function( -> " + return_type->to_string() + ")";
    }
};

inline bool Type::compatible(const std::shared_ptr<Type>& left,
                             const std::shared_ptr<Type>& right) {
    if (left == nullptr || right == nullptr) {
        return false;
    }

    if (left->kind == Kind::Type::Any || right->kind == Kind::Type::Any) {
        return true;
    }

    if (left->kind == Kind::Type::Unknown || right->kind == Kind::Type::Unknown) {
        return true;
    }

    if (left->kind != right->kind) {
        return false;
    }

    switch (left->kind) {
    case Kind::Type::Integer: {
        auto* left_integer = left->as<IntegerType>();
        auto* right_integer = right->as<IntegerType>();
        return left_integer->is_unsigned == right_integer->is_unsigned &&
               left_integer->size == right_integer->size;
    }

    case Kind::Type::Float: {
        auto* left_float = left->as<FloatType>();
        auto* right_float = right->as<FloatType>();
        return left_float->size == right_float->size;
    }

    case Kind::Type::Pointer: {
        auto* left_pointer = left->as<PointerType>();
        auto* right_pointer = right->as<PointerType>();

        if (left_pointer->element->kind == Kind::Type::Void ||
            right_pointer->element->kind == Kind::Type::Void) {
            return true;
        }

        if (left_pointer->is_mutable != right_pointer->is_mutable) {
            return false;
        }

        return compatible(left_pointer->element, right_pointer->element);
    }

    case Kind::Type::Array: {
        auto* left_array = left->as<ArrayType>();
        auto* right_array = right->as<ArrayType>();
        if (left_array->size != right_array->size) {
            return false;
        }
        return compatible(left_array->element, right_array->element);
    }

    case Kind::Type::Struct: {
        auto* left_struct = left->as<StructType>();
        auto* right_struct = right->as<StructType>();
        if (left_struct->name != right_struct->name) {
            return false;
        }
        if (left_struct->fields.size() != right_struct->fields.size()) {
            return false;
        }
        for (std::size_t i = 0; i < left_struct->fields.size(); ++i) {
            if (left_struct->fields[i]->name != right_struct->fields[i]->name) {
                return false;
            }
            if (!compatible(left_struct->fields[i]->type, right_struct->fields[i]->type)) {
                return false;
            }
        }
        return true;
    }

    case Kind::Type::Function: {
        auto* left_function = left->as<FunctionType>();
        auto* right_function = right->as<FunctionType>();
        if (!compatible(left_function->return_type, right_function->return_type)) {
            return false;
        }
        if (left_function->parameters.size() != right_function->parameters.size()) {
            return false;
        }
        for (std::size_t i = 0; i < left_function->parameters.size(); ++i) {
            if (!compatible(left_function->parameters[i]->type,
                            right_function->parameters[i]->type)) {
                return false;
            }
        }
        return true;
    }

    default:
        return true;
    }
}
