module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.sema.type;

import zep.common.arena;

export class Type {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t {
            Any,
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

  public:
    const Kind::Type kind;

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

    bool compatible(const Type* other) const;

    virtual std::string to_string() const = 0;
};

export using TypeArena = Arena<Type>;

// ---

export class GenericParameterType {
  public:
    std::string name;
    const Type* constraint;

    GenericParameterType(std::string name, const Type* constraint)
        : name(std::move(name)), constraint(constraint) {}
};

export class GenericArgumentType {
  public:
    std::string name;
    const Type* type;

    GenericArgumentType(std::string name, const Type* type) : name(std::move(name)), type(type) {}
};

export class ParameterType {
  public:
    std::string name;
    const Type* type;

    ParameterType(std::string name, const Type* type) : name(std::move(name)), type(type) {}
};

export class StructFieldType {
  public:
    std::string name;
    const Type* type;

    StructFieldType(std::string name, const Type* type) : name(std::move(name)), type(type) {}
};

// ---

export class AnyType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Any;

    explicit AnyType() : Type(static_kind) {}

    std::string to_string() const override { return "any"; }
};

export class VoidType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Void;

    explicit VoidType() : Type(static_kind) {}

    std::string to_string() const override { return "void"; }
};

export class StringType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::String;

    explicit StringType() : Type(static_kind) {}

    std::string to_string() const override { return "string"; }
};

export class BooleanType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Boolean;

    explicit BooleanType() : Type(static_kind) {}

    std::string to_string() const override { return "bool"; }
};

export class IntegerType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Integer;

    bool is_unsigned;
    std::uint8_t size;

    IntegerType(bool is_unsigned, std::uint8_t size)
        : Type(static_kind), is_unsigned(is_unsigned), size(size) {}

    std::string to_string() const override { return (is_unsigned ? "u" : "i") + std::to_string(size); }
};

export class FloatType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Float;

    std::uint8_t size;

    FloatType(std::uint8_t size) : Type(static_kind), size(size) {}

    std::string to_string() const override { return "f" + std::to_string(size); }
};

export class NamedType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Named;

    std::string name;
    std::vector<GenericArgumentType> generic_arguments;

    NamedType(std::string name, std::vector<GenericArgumentType> generic_arguments)
        : Type(static_kind), name(std::move(name)),
          generic_arguments(std::move(generic_arguments)) {}

    std::string to_string() const override {
        std::string result = name;
        if (!generic_arguments.empty()) {
            result += "[";
            for (std::size_t i = 0; i < generic_arguments.size(); ++i) {
                if (i != 0) {
                    result += ", ";
                }
                const auto& arg = generic_arguments[i];
                if (!arg.name.empty()) {
                    result += arg.name + " = ";
                }
                result += arg.type == nullptr ? std::string("unknown") : arg.type->to_string();
            }
            result += "]";
        }
        return result;
    }
};

export class ArrayType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Array;

    const Type* element;
    std::optional<std::size_t> size;

    ArrayType(const Type* element, std::optional<std::size_t> size)
        : Type(static_kind), element(element), size(size) {}

    std::string to_string() const override {
        std::string inner = element == nullptr ? std::string("unknown") : element->to_string();
        inner += "[";
        if (size.has_value()) {
            inner += std::to_string(*size);
        }
        inner += "]";
        return inner;
    }
};

export class PointerType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Pointer;

    const Type* element;
    bool is_mutable;

    PointerType(const Type* element, bool is_mutable)
        : Type(static_kind), element(element), is_mutable(is_mutable) {}

    std::string to_string() const override {
        std::string inner = element == nullptr ? std::string("unknown") : element->to_string();
        return is_mutable ? "*mut " + inner : "*" + inner;
    }
};

export class StructType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Struct;

    std::string name;

    std::vector<GenericParameterType> generic_parameters;
    std::vector<StructFieldType> fields;

    StructType(std::string name, std::vector<GenericParameterType> generic_parameters,
               std::vector<StructFieldType> fields)
        : Type(static_kind), name(std::move(name)),
          generic_parameters(std::move(generic_parameters)), fields(std::move(fields)) {}

    std::string to_string() const override {
        std::string result = name;
        if (!generic_parameters.empty()) {
            result += "<";
            for (std::size_t i = 0; i < generic_parameters.size(); ++i) {
                if (i != 0) result += ", ";
                result += generic_parameters[i].name;
            }
            result += ">";
        }
        return result;
    }
};

export class FunctionType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Function;

    std::string name;

    const Type* return_type;

    std::vector<ParameterType> parameters;
    std::vector<GenericParameterType> generic_parameters;

    bool variadic;

    FunctionType(std::string name, const Type* return_type, std::vector<ParameterType> parameters,
                 std::vector<GenericParameterType> generic_parameters, bool variadic)
        : Type(static_kind), name(std::move(name)), return_type(return_type),
          parameters(std::move(parameters)), generic_parameters(std::move(generic_parameters)),
          variadic(variadic) {}

    bool conflicts_with(const FunctionType& other) const {
        if (parameters.size() != other.parameters.size()) {
            return false;
        }

        for (std::size_t i = 0; i < parameters.size(); ++i) {
            const auto* a = parameters[i].type;
            const auto* b = other.parameters[i].type;
            if (a != nullptr && b != nullptr && !a->compatible(b)) {
                return false;
            }
        }

        return true;
    }

    std::string to_string() const override {
        std::string result = "fn";
        if (!name.empty()) {
            result += " " + name;
        }
        if (!generic_parameters.empty()) {
            result += "<";
            for (std::size_t i = 0; i < generic_parameters.size(); ++i) {
                if (i != 0) result += ", ";
                result += generic_parameters[i].name;
            }
            result += ">";
        }
        result += "(";
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (i != 0) result += ", ";
            result += parameters[i].name + ": " +
                      (parameters[i].type ? parameters[i].type->to_string() : "unknown");
        }
        if (variadic) {
            if (!parameters.empty()) result += ", ";
            result += "...";
        }
        result += "): " + (return_type ? return_type->to_string() : "unknown");
        return result;
    }
};

inline bool Type::compatible(const Type* other) const {
    if (other == nullptr) {
        return true;
    }

    if (is<AnyType>() || other->is<AnyType>()) {
        return true;
    }

    if (kind != other->kind) {
        return false;
    }

    switch (kind) {
    case Kind::Type::Integer: {
        const auto* left_integer = as<IntegerType>();
        const auto* right_integer = other->as<IntegerType>();

        return left_integer->is_unsigned == right_integer->is_unsigned &&
               left_integer->size == right_integer->size;
    }

    case Kind::Type::Float: {
        const auto* left_float = as<FloatType>();
        const auto* right_float = other->as<FloatType>();

        return left_float->size == right_float->size;
    }

    case Kind::Type::Pointer: {
        const auto* left_pointer = as<PointerType>();
        const auto* right_pointer = other->as<PointerType>();

        if (!left_pointer->is_mutable && right_pointer->is_mutable) {
            return false;
        }

        const auto* left_element = left_pointer->element;
        const auto* right_element = right_pointer->element;

        if (left_element == nullptr || right_element == nullptr) {
            return false;
        }

        if (left_element->is<VoidType>() || right_element->is<VoidType>()) {
            return true;
        }

        return left_pointer->element->compatible(right_pointer->element);
    }

    case Kind::Type::Array: {
        const auto* left_array = as<ArrayType>();
        const auto* right_array = other->as<ArrayType>();

        if (left_array->size != right_array->size) {
            return false;
        }

        const auto* left_element = left_array->element;
        const auto* right_element = right_array->element;

        if (left_element == nullptr || right_element == nullptr) {
            return false;
        }

        return left_element->compatible(right_element);
    }

    case Kind::Type::Struct: {
        const auto* left_struct = as<StructType>();
        const auto* right_struct = other->as<StructType>();

        if (left_struct->name != right_struct->name) {
            return false;
        }

        if (left_struct->fields.size() != right_struct->fields.size()) {
            return false;
        }

        for (std::size_t i = 0; i < left_struct->fields.size(); ++i) {
            if (left_struct->fields[i].name != right_struct->fields[i].name) {
                return false;
            }

            const auto* left_field_type = left_struct->fields[i].type;
            const auto* right_field_type = right_struct->fields[i].type;

            if (left_field_type == nullptr || right_field_type == nullptr) {
                return false;
            }

            if (!left_field_type->compatible(right_field_type)) {
                return false;
            }
        }
        return true;
    }

    case Kind::Type::Function: {
        const auto* left_function = as<FunctionType>();
        const auto* right_function = other->as<FunctionType>();

        const auto* left_return_type = left_function->return_type;
        const auto* right_return_type = right_function->return_type;

        if (left_return_type == nullptr || right_return_type == nullptr) {
            return false;
        }

        if (!left_return_type->compatible(right_return_type)) {
            return false;
        }

        if (left_function->parameters.size() != right_function->parameters.size()) {
            return false;
        }

        for (std::size_t i = 0; i < left_function->parameters.size(); ++i) {
            const auto* left_parameter_type = left_function->parameters[i].type;
            const auto* right_parameter_type = right_function->parameters[i].type;

            if (left_parameter_type == nullptr || right_parameter_type == nullptr) {
                return false;
            }

            if (!left_parameter_type->compatible(right_parameter_type)) {
                return false;
            }
        }

        return true;
    }

    default:
        return true;
    }
}

