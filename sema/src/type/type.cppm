module;

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

export module zep.sema.type;

import zep.common.logger;

export class Type {
  public:
    enum class Kind : std::uint8_t {
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

  protected:
    explicit Type(Kind kind) : kind(kind) {}

    Type(const Type&) = delete;
    Type& operator=(const Type&) = delete;
    Type(Type&&) = default;
    Type& operator=(Type&&) = default;

  public:
    Kind kind;

    virtual ~Type() = default;

    virtual void dump(int depth, bool with_indent = true, bool trailing_newline = true) const = 0;
    virtual std::string to_string() const = 0;

    bool is_numeric() const {
        return kind == Kind::Integer || kind == Kind::Float || kind == Kind::Unknown;
    }

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
    static constexpr Kind static_kind = Kind::Any;

    AnyType() : Type(static_kind) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "AnyType()";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const override { return "any"; }
};

export class UnknownType : public Type {
  public:
    static constexpr Kind static_kind = Kind::Unknown;

    UnknownType() : Type(static_kind) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "UnknownType()";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const override { return "unknown"; }
};

export class VoidType : public Type {
  public:
    static constexpr Kind static_kind = Kind::Void;

    VoidType() : Type(static_kind) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "VoidType()";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const override { return "void"; }
};

export class StringType : public Type {
  public:
    static constexpr Kind static_kind = Kind::String;

    StringType() : Type(static_kind) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "StringType()";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const override { return "string"; }
};

export class BooleanType : public Type {
  public:
    static constexpr Kind static_kind = Kind::Boolean;

    BooleanType() : Type(static_kind) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "BooleanType()";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const override { return "bool"; }
};

export class IntegerType : public Type {
  public:
    static constexpr Kind static_kind = Kind::Integer;

    bool is_unsigned;
    std::uint8_t size;

    IntegerType(bool is_unsigned = false, std::uint8_t size = 32)
        : Type(static_kind), is_unsigned(is_unsigned), size(size) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "IntegerType(\n";

        print_indent(depth + 1);
        std::cout << "is_unsigned: " << (is_unsigned ? "true" : "false") << ",\n";

        print_indent(depth + 1);
        std::cout << "size: " << size << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const override {
        return (is_unsigned ? "u" : "i") + std::to_string(size);
    }
};

export class FloatType : public Type {
  public:
    static constexpr Kind static_kind = Kind::Float;

    std::uint8_t size;

    explicit FloatType(std::uint8_t size = 64) : Type(static_kind), size(size) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "FloatType(size: " << size << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const override { return "f" + std::to_string(size); }
};

export class GenericParameterType {
  public:
    std::string name;
    std::shared_ptr<Type> constraint;

    GenericParameterType(std::string name, std::shared_ptr<Type> constraint = nullptr)
        : name(std::move(name)), constraint(std::move(constraint)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "GenericParameterType(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "constraint: ";
        if (constraint) {
            constraint->dump(depth + 1, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
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

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "GenericArgumentType(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "type: ";
        if (type) {
            type->dump(depth + 1, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
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

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "ParameterType(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "type: ";
        if (type) {
            type->dump(depth + 1, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const { return name + ": " + type->to_string(); }
};

export class StructFieldType {
  public:
    std::string name;
    std::shared_ptr<Type> type;

    StructFieldType(std::string name, std::shared_ptr<Type> type)
        : name(std::move(name)), type(std::move(type)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "StructFieldType(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "type: ";
        if (type) {
            type->dump(depth + 1, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const { return name + ": " + type->to_string(); }
};

export class NamedType : public Type {
  public:
    static constexpr Kind static_kind = Kind::Named;

    std::string name;
    std::vector<std::shared_ptr<GenericArgumentType>> generic_arguments;

    NamedType(std::string name,
              std::vector<std::shared_ptr<GenericArgumentType>> generic_arguments = {})
        : Type(static_kind), name(std::move(name)),
          generic_arguments(std::move(generic_arguments)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "NamedType(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "generic_arguments: [";
        if (generic_arguments.empty()) {
            std::cout << "]\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < generic_arguments.size(); ++i) {
                generic_arguments[i]->dump(depth + 2, true, false);
                std::cout << (i + 1 < generic_arguments.size() ? ",\n" : "\n");
            }
            print_indent(depth + 1);
            std::cout << "]\n";
        }

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const override { return name; }
};

export class ArrayType : public Type {
  public:
    static constexpr Kind static_kind = Kind::Array;

    std::shared_ptr<Type> element;
    std::optional<std::uint8_t> size;

    ArrayType(std::shared_ptr<Type> element, std::optional<std::uint8_t> size = std::nullopt)
        : Type(static_kind), element(std::move(element)), size(size) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "ArrayType(\n";

        print_indent(depth + 1);
        std::cout << "element: ";
        element->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "size: " << (size.has_value() ? std::to_string(*size) : "null") << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const override {
        return element->to_string() + "[" + (size.has_value() ? std::to_string(*size) : "") + "]";
    }
};

export class PointerType : public Type {
  public:
    static constexpr Kind static_kind = Kind::Pointer;

    bool is_mutable;
    std::shared_ptr<Type> element;

    explicit PointerType(std::shared_ptr<Type> element, bool is_mutable = false)
        : Type(static_kind), is_mutable(is_mutable), element(std::move(element)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "PointerType(\n";

        print_indent(depth + 1);
        std::cout << "is_mutable: " << (is_mutable ? "true" : "false") << ",\n";

        print_indent(depth + 1);
        std::cout << "element: ";
        element->dump(depth + 1, false, false);
        std::cout << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const override {
        return is_mutable ? "*mut " + element->to_string() : "*" + element->to_string();
    }
};

export class StructType : public Type {
  public:
    static constexpr Kind static_kind = Kind::Struct;

    std::string name;

    std::vector<GenericParameterType> generic_parameters;
    std::vector<StructFieldType> fields;

    StructType(std::string name, std::vector<GenericParameterType> generic_parameters,
               std::vector<StructFieldType> fields)
        : Type(static_kind), name(std::move(name)),
          generic_parameters(std::move(generic_parameters)), fields(std::move(fields)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "StructType(\n";

        print_indent(depth + 1);
        std::cout << "name: \"" << name << "\",\n";

        print_indent(depth + 1);
        std::cout << "generic_parameters: [";
        if (generic_parameters.empty()) {
            std::cout << "],\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < generic_parameters.size(); ++i) {
                generic_parameters[i].dump(depth + 2, true, false);
                std::cout << (i + 1 < generic_parameters.size() ? ",\n" : "\n");
            }

            print_indent(depth + 1);
            std::cout << "],\n";
        }
        print_indent(depth + 1);

        std::cout << "fields: [";
        if (fields.empty()) {
            std::cout << "]\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < fields.size(); ++i) {
                fields[i].dump(depth + 2, true, false);
                std::cout << (i + 1 < fields.size() ? ",\n" : "\n");
            }

            print_indent(depth + 1);
            std::cout << "]\n";
        }

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const override { return name; }
};

export class FunctionType : public Type {
  public:
    static constexpr Kind static_kind = Kind::Function;

    std::shared_ptr<Type> return_type;

    std::vector<ParameterType> parameters;
    std::vector<GenericParameterType> generic_parameters;

    bool variadic;

    FunctionType(std::shared_ptr<Type> return_type, std::vector<ParameterType> parameters,
                 std::vector<GenericParameterType> generic_parameters = {}, bool variadic = false)
        : Type(static_kind), return_type(std::move(return_type)), parameters(std::move(parameters)),
          generic_parameters(std::move(generic_parameters)), variadic(variadic) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "FunctionType(\n";

        print_indent(depth + 1);
        std::cout << "return_type: ";
        if (return_type) {
            return_type->dump(depth + 1, false, false);
        } else {
            std::cout << "null";
        }
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "parameters: [";
        if (parameters.empty()) {
            std::cout << "],\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < parameters.size(); ++i) {
                parameters[i].dump(depth + 2, true, false);
                std::cout << (i + 1 < parameters.size() ? ",\n" : "\n");
            }

            print_indent(depth + 1);
            std::cout << "],\n";
        }

        print_indent(depth + 1);
        std::cout << "generics: [";
        if (generic_parameters.empty()) {
            std::cout << "],\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < generic_parameters.size(); ++i) {
                generic_parameters[i].dump(depth + 2, true, false);
                std::cout << (i + 1 < generic_parameters.size() ? ",\n" : "\n");
            }

            print_indent(depth + 1);
            std::cout << "],\n";
        }

        print_indent(depth + 1);
        std::cout << "variadic: " << (variadic ? "true" : "false") << "\n";

        print_indent(depth);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const override {
        return "function( -> " + return_type->to_string() + ")";
    }
};
