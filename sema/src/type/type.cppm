module;

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

export module zep.sema.type;

import zep.common.logger;

export enum class TypeKind : std::uint8_t {
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
    StructLiteral,
    Function
};

export class Type {
  public:
    TypeKind kind;

    explicit Type(TypeKind kind) : kind(kind) {}
    virtual ~Type() = default;

    Type(const Type&) = delete;
    Type& operator=(const Type&) = delete;
    Type(Type&&) = default;
    Type& operator=(Type&&) = default;

    virtual void dump(int depth, bool with_indent = true, bool trailing_newline = true) const = 0;
    virtual std::string to_string() const = 0;

    bool is_numeric() const {
        return kind == TypeKind::Integer || kind == TypeKind::Float || kind == TypeKind::Unknown ||
               kind == TypeKind::Named;
    }

    bool operator==(const Type& other) const { return kind == other.kind; }
};

export class AnyType : public Type {
  public:
    AnyType() : Type(TypeKind::Any) {}

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
    UnknownType() : Type(TypeKind::Unknown) {}

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
    VoidType() : Type(TypeKind::Void) {}

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
    StringType() : Type(TypeKind::String) {}

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
    BooleanType() : Type(TypeKind::Boolean) {}

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
    bool is_unsigned;
    std::uint8_t size;

    IntegerType(bool is_unsigned = false, std::uint8_t size = 32)
        : Type(TypeKind::Integer), is_unsigned(is_unsigned), size(size) {}

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
    std::uint8_t size;

    explicit FloatType(std::uint8_t size = 64) : Type(TypeKind::Float), size(size) {}

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
    std::string name;
    std::vector<std::shared_ptr<GenericArgumentType>> generic_arguments;

    NamedType(std::string name,
              std::vector<std::shared_ptr<GenericArgumentType>> generic_arguments = {})
        : Type(TypeKind::Named), name(std::move(name)),
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
    std::shared_ptr<Type> element;
    std::optional<std::uint8_t> size;

    ArrayType(std::shared_ptr<Type> element, std::optional<std::uint8_t> size = std::nullopt)
        : Type(TypeKind::Array), element(std::move(element)), size(size) {}

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
    std::shared_ptr<Type> element;

    explicit PointerType(std::shared_ptr<Type> element)
        : Type(TypeKind::Pointer), element(std::move(element)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "PointerType(element: ";
        element->dump(depth, false, false);
        std::cout << ")";

        if (trailing_newline) {
            std::cout << "\n";
        }
    }
    std::string to_string() const override { return "*" + element->to_string(); }
};

export class StructType : public Type {
  public:
    std::string name;

    std::vector<GenericParameterType> generic_parameters;
    std::vector<StructFieldType> fields;

    StructType(std::string name, std::vector<GenericParameterType> generic_parameters,
               std::vector<StructFieldType> fields)
        : Type(TypeKind::Struct), name(std::move(name)),
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

export class StructLiteralType : public Type {
  public:
    std::shared_ptr<StructType> struct_type;
    std::vector<GenericArgumentType> generic_arguments;

    StructLiteralType(std::shared_ptr<StructType> struct_type,
                      std::vector<GenericArgumentType> generic_arguments = {})
        : Type(TypeKind::StructLiteral), struct_type(std::move(struct_type)),
          generic_arguments(std::move(generic_arguments)) {}

    void dump(int depth, bool with_indent = true, bool trailing_newline = true) const override {
        if (with_indent) {
            print_indent(depth);
        }

        std::cout << "StructLiteralType(\n";

        print_indent(depth + 1);
        std::cout << "struct_type: ";
        struct_type->dump(depth + 1, false, false);
        std::cout << ",\n";

        print_indent(depth + 1);
        std::cout << "generic_arguments: [";
        if (generic_arguments.empty()) {
            std::cout << "]\n";
        } else {
            std::cout << "\n";
            for (std::size_t i = 0; i < generic_arguments.size(); ++i) {
                generic_arguments[i].dump(depth + 2, true, false);
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
    std::string to_string() const override { return struct_type->to_string(); }
};

export class FunctionType : public Type {
  public:
    std::shared_ptr<Type> return_type;

    std::vector<ParameterType> parameters;
    std::vector<GenericParameterType> generic_parameters;

    bool variadic;

    FunctionType(std::shared_ptr<Type> return_type, std::vector<ParameterType> parameters,
                 std::vector<GenericParameterType> generic_parameters = {}, bool variadic = false)
        : Type(TypeKind::Function), return_type(std::move(return_type)),
          parameters(std::move(parameters)), generic_parameters(std::move(generic_parameters)),
          variadic(variadic) {}

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
