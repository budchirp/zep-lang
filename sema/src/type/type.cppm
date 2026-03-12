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

    virtual void dump(int depth) const = 0;
    virtual std::string to_string() const = 0;

    bool is_unknown() const { return kind == TypeKind::Unknown; }
    bool is_any() const { return kind == TypeKind::Any; }
    bool is_numeric() const {
        return kind == TypeKind::Integer || kind == TypeKind::Float || kind == TypeKind::Unknown ||
               kind == TypeKind::Named;
    }

    static bool is_compatible(const Type& expected, const Type& actual);
    static bool equal(const Type& a, const Type& b);
    static bool is_compatible_cross_kind(TypeKind expected_kind, TypeKind actual_kind);
    static bool is_compatible_same_kind(const Type& expected, const Type& actual);
};

export class AnyType : public Type {
  public:
    AnyType() : Type(TypeKind::Any) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "AnyType\n";
    }
    std::string to_string() const override { return "any"; }
};

export class UnknownType : public Type {
  public:
    UnknownType() : Type(TypeKind::Unknown) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "UnknownType\n";
    }
    std::string to_string() const override { return "unknown"; }
};

export class VoidType : public Type {
  public:
    VoidType() : Type(TypeKind::Void) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "VoidType\n";
    }
    std::string to_string() const override { return "void"; }
};

export class StringType : public Type {
  public:
    StringType() : Type(TypeKind::String) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "StringType\n";
    }
    std::string to_string() const override { return "string"; }
};

export class BooleanType : public Type {
  public:
    BooleanType() : Type(TypeKind::Boolean) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "BooleanType\n";
    }
    std::string to_string() const override { return "bool"; }
};

export class IntegerType : public Type {
  public:
    bool is_unsigned;
    std::uint16_t size;

    IntegerType(bool is_unsigned = false, std::uint16_t size = 32)
        : Type(TypeKind::Integer), is_unsigned(is_unsigned), size(size) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "IntegerType " << (is_unsigned ? "u" : "i") << size << "\n";
    }
    std::string to_string() const override {
        return (is_unsigned ? "u" : "i") + std::to_string(size);
    }
};

export class FloatType : public Type {
  public:
    std::uint16_t size;

    explicit FloatType(std::uint16_t size = 64) : Type(TypeKind::Float), size(size) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "FloatType f" << size << "\n";
    }
    std::string to_string() const override { return "f" + std::to_string(size); }
};

export class GenericParameterType {
  public:
    std::string name;
    std::shared_ptr<Type> constraint;

    GenericParameterType(std::string name, std::shared_ptr<Type> constraint = nullptr)
        : name(std::move(name)), constraint(std::move(constraint)) {}

    void dump(int depth) const {
        print_indent(depth);
        std::cout << "GenericParameterType " << name << " {\n";
        if (constraint) {
            print_indent(depth + 1);
            std::cout << "constraint: ";
            constraint->dump(depth + 1);
        }
        print_indent(depth);
        std::cout << "}\n";
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

    void dump(int depth) const {
        print_indent(depth);
        std::cout << "GenericArgumentType" << (name.empty() ? "" : " " + name) << " {\n";
        print_indent(depth + 1);
        std::cout << "type: ";
        type->dump(depth + 1);
        print_indent(depth);
        std::cout << "}\n";
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

    void dump(int depth) const {
        print_indent(depth);
        std::cout << "ParameterType " << name << " {\n";
        print_indent(depth + 1);
        std::cout << "type: ";
        type->dump(depth + 1);
        print_indent(depth);
        std::cout << "}\n";
    }
    std::string to_string() const { return name + ": " + type->to_string(); }
};

export class StructFieldType {
  public:
    std::string name;
    std::shared_ptr<Type> type;

    StructFieldType(std::string name, std::shared_ptr<Type> type)
        : name(std::move(name)), type(std::move(type)) {}

    void dump(int depth) const {
        print_indent(depth);
        std::cout << "StructFieldType " << name << " {\n";
        print_indent(depth + 1);
        std::cout << "type: ";
        type->dump(depth + 1);
        print_indent(depth);
        std::cout << "}\n";
    }
    std::string to_string() const { return name + ": " + type->to_string(); }
};

export class NamedType : public Type {
  public:
    std::string name;
    std::vector<GenericArgumentType> generics;

    NamedType(std::string name, std::vector<GenericArgumentType> generics = {})
        : Type(TypeKind::Named), name(std::move(name)), generics(std::move(generics)) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "NamedType " << name << " {\n";
        if (!generics.empty()) {
            print_indent(depth + 1);
            std::cout << "generics: [\n";
            for (const auto& generic : generics) {
                generic.dump(depth + 2);
            }
            print_indent(depth + 1);
            std::cout << "]\n";
        }
        print_indent(depth);
        std::cout << "}\n";
    }
    std::string to_string() const override { return name; }
};

export class ArrayType : public Type {
  public:
    std::shared_ptr<Type> element;
    std::optional<std::size_t> size;

    ArrayType(std::shared_ptr<Type> element, std::optional<std::size_t> size = std::nullopt)
        : Type(TypeKind::Array), element(std::move(element)), size(size) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "ArrayType {\n";
        print_indent(depth + 1);
        std::cout << "element: ";
        element->dump(depth + 1);
        print_indent(depth + 1);
        std::cout << "size: " << (size.has_value() ? std::to_string(*size) : "dynamic") << "\n";
        print_indent(depth);
        std::cout << "}\n";
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

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "PointerType {\n";
        print_indent(depth + 1);
        std::cout << "element: ";
        element->dump(depth + 1);
        print_indent(depth);
        std::cout << "}\n";
    }
    std::string to_string() const override { return "*" + element->to_string(); }
};

export class StructType : public Type {
  public:
    std::string name;
    std::vector<GenericParameterType> generics;
    std::vector<StructFieldType> fields;

    StructType(std::string name, std::vector<GenericParameterType> generics,
               std::vector<StructFieldType> fields)
        : Type(TypeKind::Struct), name(std::move(name)), generics(std::move(generics)),
          fields(std::move(fields)) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "StructType " << name << " {\n";
        if (!generics.empty()) {
            print_indent(depth + 1);
            std::cout << "generics: [\n";
            for (const auto& generic : generics) {
                generic.dump(depth + 2);
            }
            print_indent(depth + 1);
            std::cout << "]\n";
        }
        print_indent(depth + 1);
        std::cout << "fields: [\n";
        for (const auto& field : fields) {
            field.dump(depth + 2);
        }
        print_indent(depth + 1);
        std::cout << "]\n";
        print_indent(depth);
        std::cout << "}\n";
    }
    std::string to_string() const override { return name; }
};

export class StructLiteralType : public Type {
  public:
    std::shared_ptr<StructType> struct_type;
    std::vector<GenericArgumentType> generics;

    StructLiteralType(std::shared_ptr<StructType> struct_type,
                      std::vector<GenericArgumentType> generics = {})
        : Type(TypeKind::StructLiteral), struct_type(std::move(struct_type)),
          generics(std::move(generics)) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "StructLiteralType {\n";
        print_indent(depth + 1);
        std::cout << "struct_type: ";
        struct_type->dump(depth + 1);
        if (!generics.empty()) {
            print_indent(depth + 1);
            std::cout << "generics: [\n";
            for (const auto& generic : generics) {
                generic.dump(depth + 2);
            }
            print_indent(depth + 1);
            std::cout << "]\n";
        }
        print_indent(depth);
        std::cout << "}\n";
    }
    std::string to_string() const override { return struct_type->to_string(); }
};

export class FunctionType : public Type {
  public:
    std::shared_ptr<Type> return_type;
    std::vector<ParameterType> parameters;
    std::vector<GenericParameterType> generics;
    bool variadic;

    FunctionType(std::shared_ptr<Type> return_type, std::vector<ParameterType> parameters,
                 std::vector<GenericParameterType> generics = {}, bool variadic = false)
        : Type(TypeKind::Function), return_type(std::move(return_type)),
          parameters(std::move(parameters)), generics(std::move(generics)), variadic(variadic) {}

    void dump(int depth) const override {
        print_indent(depth);
        std::cout << "FunctionType {\n";
        print_indent(depth + 1);
        std::cout << "return_type: ";
        return_type->dump(depth + 1);
        print_indent(depth + 1);
        std::cout << "parameters: [\n";
        for (const auto& parameter : parameters) {
            parameter.dump(depth + 2);
        }
        print_indent(depth + 1);
        std::cout << "]\n";
        if (!generics.empty()) {
            print_indent(depth + 1);
            std::cout << "generics: [\n";
            for (const auto& generic : generics) {
                generic.dump(depth + 2);
            }
            print_indent(depth + 1);
            std::cout << "]\n";
        }
        print_indent(depth + 1);
        std::cout << "variadic: " << (variadic ? "true" : "false") << "\n";
        print_indent(depth);
        std::cout << "}\n";
    }
    std::string to_string() const override {
        return "function( -> " + return_type->to_string() + ")";
    }
};

bool Type::is_compatible_cross_kind(TypeKind expected_kind, TypeKind actual_kind) {
    if (expected_kind == TypeKind::Named || actual_kind == TypeKind::Named) {
        return (expected_kind == TypeKind::Named && actual_kind == TypeKind::Integer) ||
               (expected_kind == TypeKind::Integer && actual_kind == TypeKind::Named) ||
               (expected_kind == TypeKind::Named && actual_kind == TypeKind::Float) ||
               (expected_kind == TypeKind::Float && actual_kind == TypeKind::Named);
    }
    return false;
}

bool Type::is_compatible_same_kind(const Type& expected, const Type& actual) {
    switch (expected.kind) {
    case TypeKind::Integer: {
        const auto& e = dynamic_cast<const IntegerType&>(expected);
        const auto& a = dynamic_cast<const IntegerType&>(actual);
        return e.size == a.size && e.is_unsigned == a.is_unsigned;
    }
    case TypeKind::Float: {
        const auto& e = dynamic_cast<const FloatType&>(expected);
        const auto& a = dynamic_cast<const FloatType&>(actual);
        return e.size == a.size;
    }
    case TypeKind::String:
    case TypeKind::Boolean:
    case TypeKind::Void:
    case TypeKind::Any:
    case TypeKind::Named:
        return true;
    case TypeKind::Unknown:
        return false;
    case TypeKind::Pointer: {
        const auto& e = dynamic_cast<const PointerType&>(expected);
        const auto& a = dynamic_cast<const PointerType&>(actual);
        if (e.element->kind == TypeKind::Void) {
            return true;
        }
        return is_compatible(*e.element, *a.element);
    }
    case TypeKind::Array: {
        const auto& e = dynamic_cast<const ArrayType&>(expected);
        const auto& a = dynamic_cast<const ArrayType&>(actual);
        return is_compatible(*e.element, *a.element);
    }
    case TypeKind::Struct: {
        const auto& e = dynamic_cast<const StructType&>(expected);
        const auto& a = dynamic_cast<const StructType&>(actual);
        if (e.fields.size() != a.fields.size()) {
            return false;
        }
        for (std::size_t i = 0; i < e.fields.size(); ++i) {
            if (e.fields[i].name != a.fields[i].name) {
                return false;
            }
            if (!is_compatible(*e.fields[i].type, *a.fields[i].type)) {
                return false;
            }
        }
        return true;
    }
    case TypeKind::StructLiteral: {
        const auto& e = dynamic_cast<const StructLiteralType&>(expected);
        const auto& a = dynamic_cast<const StructLiteralType&>(actual);
        if (!is_compatible(*e.struct_type, *a.struct_type)) {
            return false;
        }
        if (e.generics.size() != a.generics.size()) {
            return false;
        }
        for (std::size_t i = 0; i < e.generics.size(); ++i) {
            if (!is_compatible(*e.generics[i].type, *a.generics[i].type)) {
                return false;
            }
        }
        return true;
    }
    case TypeKind::Function: {
        const auto& e = dynamic_cast<const FunctionType&>(expected);
        const auto& a = dynamic_cast<const FunctionType&>(actual);
        if (!is_compatible(*e.return_type, *a.return_type)) {
            return false;
        }
        if (e.parameters.size() != a.parameters.size()) {
            return false;
        }
        for (std::size_t i = 0; i < e.parameters.size(); ++i) {
            if (!is_compatible(*e.parameters[i].type, *a.parameters[i].type)) {
                return false;
            }
        }
        return true;
    }
    }
    return false;
}

bool Type::is_compatible(const Type& expected, const Type& actual) {
    if (expected.is_any() || actual.is_any()) {
        return true;
    }

    if (expected.is_unknown() || actual.is_unknown()) {
        return true;
    }

    if (expected.kind == TypeKind::StructLiteral && actual.kind == TypeKind::Struct) {
        return is_compatible(*dynamic_cast<const StructLiteralType&>(expected).struct_type, actual);
    }

    if (expected.kind == TypeKind::Struct && actual.kind == TypeKind::StructLiteral) {
        return is_compatible(expected, *dynamic_cast<const StructLiteralType&>(actual).struct_type);
    }

    if (expected.kind != actual.kind) {
        return is_compatible_cross_kind(expected.kind, actual.kind);
    }

    return is_compatible_same_kind(expected, actual);
}

bool Type::equal(const Type& a, const Type& b) {
    if (a.kind != b.kind) {
        return false;
    }

    switch (a.kind) {
    case TypeKind::Integer: {
        const auto& ai = dynamic_cast<const IntegerType&>(a);
        const auto& bi = dynamic_cast<const IntegerType&>(b);
        return ai.size == bi.size && ai.is_unsigned == bi.is_unsigned;
    }
    case TypeKind::Float: {
        const auto& af = dynamic_cast<const FloatType&>(a);
        const auto& bf = dynamic_cast<const FloatType&>(b);
        return af.size == bf.size;
    }
    case TypeKind::Pointer: {
        const auto& ap = dynamic_cast<const PointerType&>(a);
        const auto& bp = dynamic_cast<const PointerType&>(b);
        return equal(*ap.element, *bp.element);
    }
    case TypeKind::Array: {
        const auto& aa = dynamic_cast<const ArrayType&>(a);
        const auto& ba = dynamic_cast<const ArrayType&>(b);
        return equal(*aa.element, *ba.element);
    }
    case TypeKind::Named: {
        const auto& an = dynamic_cast<const NamedType&>(a);
        const auto& bn = dynamic_cast<const NamedType&>(b);
        return an.name == bn.name;
    }
    default:
        return true;
    }
}
