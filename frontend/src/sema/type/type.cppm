module;

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.sema.type;

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

    std::string to_string() const;
};

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
};

export class VoidType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Void;

    explicit VoidType() : Type(static_kind) {}
};

export class StringType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::String;

    explicit StringType() : Type(static_kind) {}
};

export class BooleanType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Boolean;

    explicit BooleanType() : Type(static_kind) {}
};

export class IntegerType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Integer;

    bool is_unsigned;
    std::uint8_t size;

    IntegerType(bool is_unsigned, std::uint8_t size)
        : Type(static_kind), is_unsigned(is_unsigned), size(size) {}
};

export class FloatType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Float;

    std::uint8_t size;

    FloatType(std::uint8_t size) : Type(static_kind), size(size) {}
};

export class NamedType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Named;

    std::string name;
    std::vector<GenericArgumentType> generic_arguments;

    NamedType(std::string name, std::vector<GenericArgumentType> generic_arguments)
        : Type(static_kind), name(std::move(name)),
          generic_arguments(std::move(generic_arguments)) {}
};

export class ArrayType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Array;

    const Type* element;
    std::optional<std::size_t> size;

    ArrayType(const Type* element, std::optional<std::size_t> size)
        : Type(static_kind), element(element), size(size) {}
};

export class PointerType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Pointer;

    const Type* element;
    bool is_mutable;

    PointerType(const Type* element, bool is_mutable)
        : Type(static_kind), element(element), is_mutable(is_mutable) {}
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

    bool conflicts_with(const FunctionType& other) const;
};

inline bool Type::compatible(const Type* other) const {
    if (other == nullptr) {
        return true;
    }

    if (kind == Kind::Type::Any || other->kind == Kind::Type::Any) {
        return true;
    }

    if (kind != other->kind) {
        return false;
    }

    switch (kind) {
    case Kind::Type::Integer: {
        const IntegerType* left_integer = as<IntegerType>();
        const IntegerType* right_integer = other->as<IntegerType>();
        return left_integer->is_unsigned == right_integer->is_unsigned &&
               left_integer->size == right_integer->size;
    }

    case Kind::Type::Float: {
        const FloatType* left_float = as<FloatType>();
        const FloatType* right_float = other->as<FloatType>();
        return left_float->size == right_float->size;
    }

    case Kind::Type::Pointer: {
        const PointerType* left_pointer = as<PointerType>();
        const PointerType* right_pointer = other->as<PointerType>();

        const Type* left_element = left_pointer->element;
        const Type* right_element = right_pointer->element;
        if (left_element->kind == Kind::Type::Void || right_element->kind == Kind::Type::Void) {
            return true;
        }

        if (left_pointer->is_mutable != right_pointer->is_mutable) {
            return false;
        }

        return left_pointer->element->compatible(right_pointer->element);
    }

    case Kind::Type::Array: {
        const ArrayType* left_array = as<ArrayType>();
        const ArrayType* right_array = other->as<ArrayType>();
        if (left_array->size != right_array->size) {
            return false;
        }
        return left_array->element->compatible(right_array->element);
    }

    case Kind::Type::Struct: {
        const StructType* left_struct = as<StructType>();
        const StructType* right_struct = other->as<StructType>();
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
            const Type* lf = left_struct->fields[i].type;
            const Type* rf = right_struct->fields[i].type;
            if (!(lf == nullptr || rf == nullptr || lf->compatible(rf))) {
                return false;
            }
        }
        return true;
    }

    case Kind::Type::Function: {
        const FunctionType* left_function = as<FunctionType>();
        const FunctionType* right_function = other->as<FunctionType>();
        const Type* lr = left_function->return_type;
        const Type* rr = right_function->return_type;
        if (!(lr == nullptr || rr == nullptr || lr->compatible(rr))) {
            return false;
        }
        if (left_function->parameters.size() != right_function->parameters.size()) {
            return false;
        }
        for (std::size_t i = 0; i < left_function->parameters.size(); ++i) {
            const Type* lp = left_function->parameters[i].type;
            const Type* rp = right_function->parameters[i].type;
            if (lp != nullptr && rp != nullptr && !lp->compatible(rp)) {
                return false;
            }
        }
        return true;
    }

    default:
        return true;
    }
}

inline bool FunctionType::conflicts_with(const FunctionType& other) const {
    if (parameters.size() != other.parameters.size()) {
        return false;
    }

    for (std::size_t i = 0; i < parameters.size(); ++i) {
        const Type* a = parameters[i].type;
        const Type* b = other.parameters[i].type;
        if (a != nullptr && b != nullptr && !a->compatible(b)) {
            return false;
        }
    }

    return true;
}

inline std::string Type::to_string() const {
    switch (kind) {
    case Kind::Type::Any:
        return "any";
    case Kind::Type::Void:
        return "void";
    case Kind::Type::String:
        return "string";
    case Kind::Type::Boolean:
        return "bool";
    case Kind::Type::Integer: {
        const IntegerType* integer = as<IntegerType>();
        return (integer->is_unsigned ? "u" : "i") + std::to_string(integer->size);
    }
    case Kind::Type::Float: {
        const FloatType* float_type = as<FloatType>();
        return "f" + std::to_string(float_type->size);
    }
    case Kind::Type::Named: {
        const NamedType* named = as<NamedType>();
        std::string result = named->name;
        if (!named->generic_arguments.empty()) {
            result += "[";
            for (std::size_t i = 0; i < named->generic_arguments.size(); ++i) {
                if (i != 0) {
                    result += ", ";
                }
                const GenericArgumentType& arg = named->generic_arguments[i];
                if (!arg.name.empty()) {
                    result += arg.name + " = ";
                }
                const auto arg_type = arg.type;
                result += arg_type == nullptr ? std::string("unknown") : arg_type->to_string();
            }
            result += "]";
        }
        return result;
    }
    case Kind::Type::Array: {
        const ArrayType* array = as<ArrayType>();
        const auto element = array->element;
        std::string inner = element == nullptr ? std::string("unknown") : element->to_string();
        inner += "[";
        if (array->size.has_value()) {
            inner += std::to_string(*array->size);
        }
        inner += "]";
        return inner;
    }
    case Kind::Type::Pointer: {
        const PointerType* pointer = as<PointerType>();
        const auto element = pointer->element;
        std::string inner = element == nullptr ? std::string("unknown") : element->to_string();
        return pointer->is_mutable ? "*mut " + inner : "*" + inner;
    }
    case Kind::Type::Struct: {
        const StructType* struct_type = as<StructType>();
        return struct_type->name;
    }
    case Kind::Type::Function: {
        const FunctionType* function_type = as<FunctionType>();
        const auto return_type = function_type->return_type;
        std::string ret =
            return_type == nullptr ? std::string("unknown") : return_type->to_string();
        return "function( -> " + ret + ")";
    }

    default:
        return "?";
    }
}
