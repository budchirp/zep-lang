module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

export module zep.frontend.sema.type;

export import :base;
export import :value;
import zep.frontend.sema.kind;

export class GenericParameterType {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t {
            Type,
            Const,
        };
    };

    Kind::Type kind;
    std::string name;
    const Type* type;
    const void* declaration;

    GenericParameterType(std::string name, const Type* type, const void* declaration = nullptr)
        : kind(Kind::Type::Type), name(std::move(name)), type(type), declaration(declaration) {}

    GenericParameterType(Kind::Type kind, std::string name, const Type* type,
                         const void* declaration = nullptr)
        : kind(kind), name(std::move(name)), type(type), declaration(declaration) {}

    bool is_const() const { return kind == Kind::Type::Const; }
};

export class GenericArgumentType {
  public:
    std::string name;
    const Type* type;
    std::optional<ConstBinding> const_binding;

    GenericArgumentType(std::string name, const Type* type)
        : name(std::move(name)), type(type), const_binding(std::nullopt) {}

    GenericArgumentType(std::string name, CompileTimeValue compile_time_value)
        : GenericArgumentType(std::move(name), ConstBinding(std::move(compile_time_value))) {}

    GenericArgumentType(std::string name, ConstBinding binding)
        : name(std::move(name)), type(binding.type), const_binding(std::move(binding)) {}

    GenericArgumentType(std::string name, GenericBinding binding)
        : name(std::move(name)), type(nullptr), const_binding(std::nullopt) {
        if (const auto* argument = std::get_if<TypeBinding>(&binding); argument != nullptr) {
            type = argument->type;
        } else {
            const_binding = std::get<ConstBinding>(std::move(binding));
            type = const_binding->type;
        }
    }

    bool is_const() const { return const_binding.has_value(); }

    GenericBinding binding() const {
        return const_binding.has_value() ? GenericBinding(*const_binding)
                                         : GenericBinding(TypeBinding(type));
    }
};

export class ParameterType {
  public:
    std::string name;
    const Type* type;

    ParameterType(std::string name, const Type* type) : name(std::move(name)), type(type) {}
};

export class MethodType {
  public:
    std::string name;
    const FunctionType* type;
    std::size_t index;

    MethodType(std::string name, const FunctionType* type, std::size_t index)
        : name(std::move(name)), type(type), index(index) {}
};

export class FieldType {
  public:
    std::string name;
    const Type* type;
    Visibility::Type visibility;

    FieldType(std::string name, const Type* type,
              Visibility::Type visibility = Visibility::Type::Public)
        : name(std::move(name)), type(type), visibility(visibility) {}
};

export class EnumVariantType {
  public:
    std::string name;
    std::size_t index;
    std::vector<FieldType> fields;
    std::optional<CompileTimeValue> discriminant;

    EnumVariantType(std::string name, std::size_t index, std::vector<FieldType> fields,
                    std::optional<CompileTimeValue> discriminant = std::nullopt)
        : name(std::move(name)), index(index), fields(std::move(fields)),
          discriminant(std::move(discriminant)) {}

    const FieldType* find_field(const std::string& field_name) const {
        for (const auto& field : fields) {
            if (field.name == field_name) {
                return &field;
            }
        }

        return nullptr;
    }
};

export class AnyType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Any;

    explicit AnyType() : Type(static_kind, "any") {}

    std::size_t byte_size() const override { return 8; }
    std::size_t alignment() const override { return 8; }

    std::string to_string() const override { return "any"; }
};

export class VoidType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Void;

    explicit VoidType() : Type(static_kind, "void") {}

    std::size_t byte_size() const override { return 0; }
    std::size_t alignment() const override { return 1; }

    std::string to_string() const override { return "void"; }
};

export class NeverType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Never;

    explicit NeverType() : Type(static_kind, "never") {}

    std::size_t byte_size() const override { return 0; }
    std::size_t alignment() const override { return 1; }

    std::string to_string() const override { return "never"; }
};

export class StringType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::String;

    explicit StringType() : Type(static_kind, "cstr") {}

    std::size_t byte_size() const override { return 8; }
    std::size_t alignment() const override { return 8; }

    std::string to_string() const override { return "cstr"; }
};

export class BooleanType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Boolean;

    explicit BooleanType() : Type(static_kind, "boolean") {}

    std::size_t byte_size() const override { return 1; }
    std::size_t alignment() const override { return 1; }

    std::string to_string() const override { return "boolean"; }
};

export class CharType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Char;

    explicit CharType() : Type(static_kind, "char") {}

    bool is_unsigned_type() const override { return true; }
    std::uint8_t bit_width() const override { return 8; }

    std::size_t byte_size() const override { return 1; }
    std::size_t alignment() const override { return 1; }

    std::string to_string() const override { return "char"; }
};

export class IntegerType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Integer;

    bool is_unsigned;
    std::uint8_t size;

    IntegerType(bool is_unsigned, std::uint8_t size)
        : Type(static_kind, "integer"), is_unsigned(is_unsigned), size(size) {}

    bool is_unsigned_type() const override { return is_unsigned; }
    std::uint8_t bit_width() const override { return size; }

    std::size_t byte_size() const override { return static_cast<std::size_t>(size) / 8; }
    std::size_t alignment() const override { return byte_size(); }

    std::string to_string() const override {
        return (is_unsigned ? "u" : "i") + std::to_string(size);
    }
};

export class FloatType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Float;

    std::uint8_t size;

    FloatType(std::uint8_t size) : Type(static_kind, "float"), size(size) {}

    std::size_t byte_size() const override { return static_cast<std::size_t>(size) / 8; }
    std::size_t alignment() const override { return byte_size(); }

    std::string to_string() const override { return "f" + std::to_string(size); }
};

export class NamedType : public Type {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Named;

    std::string name;
    std::vector<GenericArgumentType> generic_arguments;
    const void* declaration;

    NamedType(std::string name, std::vector<GenericArgumentType> generic_arguments,
              const void* declaration = nullptr)
        : Type(static_kind, "named"), name(std::move(name)),
          generic_arguments(std::move(generic_arguments)), declaration(declaration) {}

    std::size_t byte_size() const override { return 0; }
    std::size_t alignment() const override { return 1; }

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
                if (arg.is_const()) {
                    result += arg.const_binding->to_string();
                } else {
                    result += arg.type == nullptr ? std::string("unknown") : arg.type->to_string();
                }
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
    ArrayExtent extent;

    ArrayType(const Type* element, ArrayExtent extent)
        : Type(static_kind, "array"), element(element), extent(std::move(extent)) {}

    ArrayType(const Type* element, std::size_t size)
        : Type(static_kind, "array"), element(element), extent(ConcreteArrayExtent(size)) {}

    std::size_t byte_size() const override {
        const auto* concrete = std::get_if<ConcreteArrayExtent>(&extent);
        if (concrete == nullptr || element == nullptr) {
            return 0;
        }
        return element->byte_size() * concrete->value;
    }

    std::size_t alignment() const override { return element != nullptr ? element->alignment() : 1; }

    bool is_copy() const override { return element != nullptr && element->is_copy(); }

    std::string to_string() const override {
        std::string inner = element == nullptr ? std::string("unknown") : element->to_string();
        inner += "[";
        if (const auto* concrete = std::get_if<ConcreteArrayExtent>(&extent); concrete != nullptr) {
            inner += std::to_string(concrete->value);
        } else if (std::holds_alternative<DependentArrayExtent>(extent)) {
            inner += "<dependent>";
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
        : Type(static_kind, "pointer"), element(element), is_mutable(is_mutable) {}

    std::size_t byte_size() const override { return 8; }
    std::size_t alignment() const override { return 8; }

    std::string to_string() const override {
        std::string inner = element == nullptr ? std::string("unknown") : element->to_string();
        return is_mutable ? "*mut " + inner : "*" + inner;
    }
};

export class NominalType : public Type {
  public:
    const NominalType* const definition;
    std::string name;
    std::vector<GenericParameterType> generic_parameters;
    std::vector<GenericArgumentType> generic_arguments;
    void* member_scope = nullptr;

    NominalType(Kind::Type kind, std::string label, std::string name,
                std::vector<GenericParameterType> generic_parameters,
                std::vector<GenericArgumentType> generic_arguments,
                const NominalType* definition = nullptr)
        : Type(kind, std::move(label)), definition(definition != nullptr ? definition : this),
          name(std::move(name)), generic_parameters(std::move(generic_parameters)),
          generic_arguments(std::move(generic_arguments)) {}

    std::string generic_suffix() const {
        if (!generic_arguments.empty()) {
            std::string result = "<";
            for (std::size_t i = 0; i < generic_arguments.size(); ++i) {
                if (i != 0) {
                    result += ", ";
                }
                if (generic_arguments[i].is_const()) {
                    result += generic_arguments[i].const_binding->to_string();
                } else if (generic_arguments[i].type != nullptr) {
                    result += generic_arguments[i].type->to_string();
                } else {
                    result += "unknown";
                }
            }
            result += ">";
            return result;
        }

        if (!generic_parameters.empty()) {
            std::string result = "<";
            for (std::size_t i = 0; i < generic_parameters.size(); ++i) {
                if (i != 0) {
                    result += ", ";
                }
                result += generic_parameters[i].name;
            }
            result += ">";
            return result;
        }

        return {};
    }

    std::string to_string() const override { return name + generic_suffix(); }

    bool same_nominal(const NominalType& other) const {
        if (name != other.name) {
            return false;
        }

        if (generic_arguments.empty() || other.generic_arguments.empty()) {
            return true;
        }

        if (generic_arguments.size() != other.generic_arguments.size()) {
            return false;
        }

        for (std::size_t index = 0; index < generic_arguments.size(); ++index) {
            const auto left_compile_time = generic_arguments[index].is_const();
            const auto right_compile_time = other.generic_arguments[index].is_const();
            if (left_compile_time || right_compile_time) {
                if (left_compile_time != right_compile_time) {
                    return false;
                }
                if (!generic_arguments[index].const_binding->is_concrete() &&
                    !other.generic_arguments[index].const_binding->is_concrete()) {
                    continue;
                }
                if (generic_arguments[index].const_binding !=
                    other.generic_arguments[index].const_binding) {
                    return false;
                }
                continue;
            }

            const auto* left = generic_arguments[index].type;
            const auto* right = other.generic_arguments[index].type;
            if (left == nullptr || right == nullptr || !left->accepts(right)) {
                return false;
            }
        }

        return true;
    }
};

export class InterfaceType : public NominalType {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Interface;

    std::vector<MethodType> methods;
    std::vector<const InterfaceType*> interfaces;

    InterfaceType(std::string name, std::vector<GenericParameterType> generic_parameters,
                  std::vector<MethodType> methods,
                  std::vector<const InterfaceType*> interfaces = {},
                  std::vector<GenericArgumentType> generic_arguments = {},
                  const NominalType* definition = nullptr)
        : NominalType(static_kind, "interface", std::move(name), std::move(generic_parameters),
                      std::move(generic_arguments), definition),
          methods(std::move(methods)), interfaces(std::move(interfaces)) {}

    const MethodType* find_method(const std::string& method_name) const {
        for (const auto& method : methods) {
            if (method.name == method_name) {
                return &method;
            }
        }

        return nullptr;
    }

    bool inherits_from(const InterfaceType* target) const {
        for (const auto* interface_type : interfaces) {
            if (interface_type == nullptr) {
                continue;
            }

            if (interface_type->same_nominal(*target) || interface_type->inherits_from(target)) {
                return true;
            }
        }

        return false;
    }

    std::size_t byte_size() const override { return 16; }
    std::size_t alignment() const override { return 8; }
};

export class StructType : public NominalType {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Struct;

    std::vector<FieldType> fields;
    std::vector<MethodType> methods;
    const StructType* base_type;
    std::vector<const InterfaceType*> interfaces;

    StructType(std::string name, std::vector<GenericParameterType> generic_parameters,
               std::vector<FieldType> fields,
               std::vector<GenericArgumentType> generic_arguments = {},
               const StructType* base_type = nullptr,
               std::vector<const InterfaceType*> interfaces = {},
               std::vector<MethodType> methods = {}, const NominalType* definition = nullptr)
        : NominalType(static_kind, "struct", std::move(name), std::move(generic_parameters),
                      std::move(generic_arguments), definition),
          fields(std::move(fields)), methods(std::move(methods)), base_type(base_type),
          interfaces(std::move(interfaces)) {}

    const FieldType* find_field(const std::string& field_name) const {
        for (const auto& field : fields) {
            if (field.name == field_name) {
                return &field;
            }
        }

        return nullptr;
    }

    std::optional<std::size_t> field_index(const std::string& field_name) const {
        for (std::size_t index = 0; index < fields.size(); ++index) {
            if (fields[index].name == field_name) {
                return index;
            }
        }

        return std::nullopt;
    }

    bool is_copy() const override {
        for (const auto* interface_type : interfaces) {
            if (interface_type != nullptr && interface_type->name == "Copy") {
                return true;
            }
        }

        if (base_type != nullptr) {
            return base_type->is_copy();
        }

        return false;
    }

    bool inherits_from(const StructType* target) const {
        auto* current = base_type;

        while (current != nullptr) {
            if (current->same_nominal(*target)) {
                return true;
            }

            current = current->base_type;
        }

        return false;
    }

    bool implements(const InterfaceType* target) const {
        for (const auto* interface_type : interfaces) {
            if (interface_type != nullptr &&
                (interface_type->same_nominal(*target) || interface_type->inherits_from(target))) {
                return true;
            }
        }

        if (base_type != nullptr) {
            return base_type->implements(target);
        }

        return false;
    }

    std::size_t byte_size() const override {
        auto total = std::size_t{0};
        auto maximum_alignment = std::size_t{1};
        for (const auto& field : fields) {
            if (field.type != nullptr) {
                auto field_alignment = field.type->alignment();
                total = align_to(total, field_alignment);
                total += field.type->byte_size();
                maximum_alignment = std::max(maximum_alignment, field_alignment);
            }
        }
        return align_to(total, maximum_alignment);
    }

    std::size_t alignment() const override {
        auto maximum_alignment = std::size_t{1};
        for (const auto& field : fields) {
            if (field.type != nullptr) {
                maximum_alignment = std::max(maximum_alignment, field.type->alignment());
            }
        }
        return maximum_alignment;
    }
};

export class EnumType : public NominalType {
  public:
    static constexpr Kind::Type static_kind = Kind::Type::Enum;

    const Type* backing_type;
    std::vector<EnumVariantType> variants;
    std::vector<const InterfaceType*> interfaces;
    std::vector<MethodType> methods;

    EnumType(std::string name, std::vector<GenericParameterType> generic_parameters,
             std::vector<EnumVariantType> variants, const Type* backing_type = nullptr,
             std::vector<GenericArgumentType> generic_arguments = {},
             std::vector<const InterfaceType*> interfaces = {},
             std::vector<MethodType> methods = {}, const NominalType* definition = nullptr)
        : NominalType(static_kind, "enum", std::move(name), std::move(generic_parameters),
                      std::move(generic_arguments), definition),
          backing_type(backing_type), variants(std::move(variants)),
          interfaces(std::move(interfaces)), methods(std::move(methods)) {}

    const EnumVariantType* find_variant(const std::string& variant_name) const {
        for (const auto& variant : variants) {
            if (variant.name == variant_name) {
                return &variant;
            }
        }

        return nullptr;
    }

    std::optional<std::size_t> payload_index(const std::string& variant_name,
                                             const std::string& field_name) const {
        auto index = std::size_t{1};

        for (const auto& variant : variants) {
            for (const auto& field : variant.fields) {
                if (variant.name == variant_name && field.name == field_name) {
                    return index;
                }

                index = index + 1;
            }
        }

        return std::nullopt;
    }

    bool is_copy() const override { return true; }

    std::size_t byte_size() const override {
        if (backing_type != nullptr) {
            return backing_type->byte_size();
        }

        auto total = std::size_t{4};

        for (const auto& variant : variants) {
            for (const auto& field : variant.fields) {
                if (field.type != nullptr) {
                    total = align_to(total, field.type->alignment());
                    total += field.type->byte_size();
                }
            }
        }

        return align_to(total, alignment());
    }

    std::size_t alignment() const override {
        if (backing_type != nullptr) {
            return backing_type->alignment();
        }

        auto maximum_alignment = std::size_t{4};

        for (const auto& variant : variants) {
            for (const auto& field : variant.fields) {
                if (field.type != nullptr) {
                    maximum_alignment = std::max(maximum_alignment, field.type->alignment());
                }
            }
        }

        return maximum_alignment;
    }
};

bool Type::is_scalar() const {
    return is<IntegerType>() || is<FloatType>() || is<BooleanType>() || is<CharType>() ||
           is<StringType>();
}

const NominalType* Type::as_nominal() const {
    if (const auto* struct_type = as<StructType>(); struct_type != nullptr) {
        return struct_type;
    }

    if (const auto* enum_type = as<EnumType>(); enum_type != nullptr) {
        return enum_type;
    }

    if (const auto* interface_type = as<InterfaceType>(); interface_type != nullptr) {
        return interface_type;
    }

    return nullptr;
}

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
        : Type(static_kind, "function"), name(std::move(name)), return_type(return_type),
          parameters(std::move(parameters)), generic_parameters(std::move(generic_parameters)),
          variadic(variadic) {}

    std::size_t byte_size() const override { return 8; }
    std::size_t alignment() const override { return 8; }

    bool conflicts_with(const FunctionType& other) const {
        if (parameters.size() != other.parameters.size()) {
            return false;
        }

        for (std::size_t i = 0; i < parameters.size(); ++i) {
            const auto* a = parameters[i].type;
            const auto* b = other.parameters[i].type;
            if (a != nullptr && b != nullptr && !a->accepts(b)) {
                return false;
            }
        }

        return true;
    }

    std::string to_string() const override {
        std::string result = "fn";
        if (!generic_parameters.empty()) {
            result += "<";
            for (std::size_t i = 0; i < generic_parameters.size(); ++i) {
                if (i != 0) {
                    result += ", ";
                }
                result += generic_parameters[i].name;
            }
            result += ">";
        }
        result += "(";
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (i != 0) {
                result += ", ";
            }
            result += (parameters[i].type ? parameters[i].type->to_string() : "unknown");
        }
        if (variadic) {
            if (!parameters.empty()) {
                result += ", ";
            }
            result += "...";
        }
        result += "): ";
        result += (return_type ? return_type->to_string() : "unknown");
        return result;
    }
};

bool Type::fields_accept(const std::vector<FieldType>& left, const std::vector<FieldType>& right) {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t i = 0; i < left.size(); ++i) {
        if (left[i].name != right[i].name) {
            return false;
        }

        if (left[i].type == nullptr || right[i].type == nullptr) {
            return false;
        }

        if (!left[i].type->accepts(right[i].type)) {
            return false;
        }
    }

    return true;
}

bool Type::same(const Type* other) const {
    if (this == other) {
        return true;
    }

    if (other == nullptr || kind != other->kind) {
        return false;
    }

    if (const auto* integer = as<IntegerType>(); integer != nullptr) {
        const auto* right = other->as<IntegerType>();
        return integer->size == right->size && integer->is_unsigned == right->is_unsigned;
    }

    if (const auto* floating = as<FloatType>(); floating != nullptr) {
        return floating->size == other->as<FloatType>()->size;
    }

    if (const auto* named = as<NamedType>(); named != nullptr) {
        return named->declaration != nullptr &&
               named->declaration == other->as<NamedType>()->declaration;
    }

    if (const auto* array = as<ArrayType>(); array != nullptr) {
        const auto* right = other->as<ArrayType>();
        return array->extent == right->extent && array->element != nullptr &&
               array->element->same(right->element);
    }

    if (const auto* pointer = as<PointerType>(); pointer != nullptr) {
        const auto* right = other->as<PointerType>();
        return pointer->is_mutable == right->is_mutable && pointer->element != nullptr &&
               pointer->element->same(right->element);
    }

    if (const auto* nominal = as_nominal(); nominal != nullptr) {
        const auto* right = other->as_nominal();
        if (nominal->definition != right->definition ||
            nominal->generic_arguments.size() != right->generic_arguments.size()) {
            return false;
        }
        for (std::size_t index = 0; index < nominal->generic_arguments.size(); ++index) {
            if (nominal->generic_arguments[index].binding() !=
                right->generic_arguments[index].binding()) {
                return false;
            }
        }
        return true;
    }

    return is<BooleanType>() || is<CharType>() || is<StringType>() || is<VoidType>() ||
           is<NeverType>() || is<AnyType>();
}

std::size_t Type::hash() const {
    if (const auto* nominal = as_nominal(); nominal != nullptr) {
        auto result = std::hash<const void*>{}(nominal->definition);
        for (const auto& argument : nominal->generic_arguments) {
            result ^= GenericBindingHash{}(argument.binding()) + 0x9e3779b9 + (result << 6) +
                      (result >> 2);
        }
        return result;
    }
    return std::hash<std::string>{}(to_string());
}

bool Type::accepts(const Type* other) const {
    if (other == nullptr) {
        return true;
    }

    if (is<AnyType>() || other->is<AnyType>()) {
        return true;
    }

    if (is<NeverType>() || other->is<NeverType>()) {
        return true;
    }

    if (const auto *left_pointer = as<PointerType>(), *right_pointer = other->as<PointerType>();
        left_pointer != nullptr && right_pointer != nullptr) {
        if (left_pointer->is_mutable && !right_pointer->is_mutable) {
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

        if (const auto* right_array = right_element->as<ArrayType>(); right_array != nullptr) {
            return left_element->accepts(right_array->element);
        }

        if (const auto* left_array = left_element->as<ArrayType>(); left_array != nullptr) {
            return left_array->element->accepts(right_element);
        }

        return left_element->accepts(right_element);
    }

    const auto* left_interface = as<InterfaceType>();
    const auto* right_struct_for_interface = other->as<StructType>();
    if (left_interface != nullptr && right_struct_for_interface != nullptr) {
        return right_struct_for_interface->implements(left_interface);
    }

    const auto* right_pointer_for_interface = other->as<PointerType>();
    const auto* right_pointer_struct = right_pointer_for_interface != nullptr
                                           ? right_pointer_for_interface->element->as<StructType>()
                                           : nullptr;
    if (left_interface != nullptr && right_pointer_struct != nullptr) {
        return right_pointer_struct->implements(left_interface);
    }

    const auto* left_struct_for_base = as<StructType>();
    const auto* right_struct_for_base = other->as<StructType>();
    if (left_struct_for_base != nullptr && right_struct_for_base != nullptr &&
        right_struct_for_base->inherits_from(left_struct_for_base)) {
        return true;
    }

    if (kind != other->kind) {
        return false;
    }

    switch (kind) {
    case Type::Kind::Type::Any:
    case Type::Kind::Type::Never:
    case Type::Kind::Type::Void:
    case Type::Kind::Type::String:
    case Type::Kind::Type::Boolean:
    case Type::Kind::Type::Char:
        return true;

    case Type::Kind::Type::Integer: {
        const auto* left_integer = as<IntegerType>();
        const auto* right_integer = other->as<IntegerType>();

        if (left_integer == nullptr || right_integer == nullptr) {
            return false;
        }

        return left_integer->is_unsigned == right_integer->is_unsigned &&
               left_integer->size == right_integer->size;
    }

    case Type::Kind::Type::Float: {
        const auto* left_float = as<FloatType>();
        const auto* right_float = other->as<FloatType>();

        if (left_float == nullptr || right_float == nullptr) {
            return false;
        }

        return left_float->size == right_float->size;
    }

    case Type::Kind::Type::Pointer: {
        const auto* left_pointer = as<PointerType>();
        const auto* right_pointer = other->as<PointerType>();

        if (left_pointer->is_mutable && !right_pointer->is_mutable) {
            return false;
        }

        if (left_pointer->element == nullptr || right_pointer->element == nullptr) {
            return false;
        }

        if (left_pointer->element->is<VoidType>() || right_pointer->element->is<VoidType>()) {
            return true;
        }

        return left_pointer->element->accepts(right_pointer->element);
    }

    case Type::Kind::Type::Array: {
        const auto* left_array = as<ArrayType>();
        const auto* right_array = other->as<ArrayType>();

        const auto left_dependent =
            std::holds_alternative<DependentArrayExtent>(left_array->extent);
        const auto right_dependent =
            std::holds_alternative<DependentArrayExtent>(right_array->extent);
        if (left_array->extent != right_array->extent && !(left_dependent && right_dependent)) {
            return false;
        }

        const auto* left_element = left_array->element;
        const auto* right_element = right_array->element;

        if (left_element == nullptr || right_element == nullptr) {
            return false;
        }

        return left_element->accepts(right_element);
    }

    case Type::Kind::Type::Struct: {
        const auto* left_struct = as<StructType>();
        const auto* right_struct = other->as<StructType>();

        return left_struct->same_nominal(*right_struct) || right_struct->inherits_from(left_struct);
    }

    case Type::Kind::Type::Interface: {
        const auto* left_interface = as<InterfaceType>();
        const auto* right_interface = other->as<InterfaceType>();

        return left_interface->same_nominal(*right_interface) ||
               right_interface->inherits_from(left_interface);
    }

    case Type::Kind::Type::Enum: {
        const auto* left_enum = as<EnumType>();
        const auto* right_enum = other->as<EnumType>();

        return left_enum->same_nominal(*right_enum);
    }

    case Type::Kind::Type::Named: {
        const auto* left_named = as<NamedType>();
        const auto* right_named = other->as<NamedType>();

        return left_named->name == right_named->name;
    }

    case Type::Kind::Type::Function: {
        const auto* left_function = as<FunctionType>();
        const auto* right_function = other->as<FunctionType>();

        if (left_function->return_type == nullptr || right_function->return_type == nullptr) {
            return false;
        }

        if (!left_function->return_type->accepts(right_function->return_type)) {
            return false;
        }

        if (left_function->parameters.size() != right_function->parameters.size()) {
            return false;
        }

        for (std::size_t i = 0; i < left_function->parameters.size(); ++i) {
            if (left_function->parameters[i].type == nullptr ||
                right_function->parameters[i].type == nullptr) {
                return false;
            }

            if (!left_function->parameters[i].type->accepts(right_function->parameters[i].type)) {
                return false;
            }
        }

        return true;
    }
    }

    return false;
}
