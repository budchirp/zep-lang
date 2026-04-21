module;

#include <cstddef>
#include <optional>
#include <string>

export module zep.frontend.sema.type.type_helper;

import zep.frontend.sema.type;
import zep.frontend.sema.type.type_id;
import zep.frontend.arena.type;

export class TypeHelper {
  private:
    const TypeArena& types;

  public:
    explicit TypeHelper(const TypeArena& types) : types(types) {}

    bool compatible(TypeId left, TypeId right) const {
        if (left == right) {
            return true;
        }

        const Type* left_type = types.get(left);
        const Type* right_type = types.get(right);

        if (left_type == nullptr || right_type == nullptr) {
            return false;
        }

        if (left_type->kind == Type::Kind::Type::Any || right_type->kind == Type::Kind::Type::Any) {
            return true;
        }

        if (left_type->kind == Type::Kind::Type::Unknown ||
            right_type->kind == Type::Kind::Type::Unknown) {
            return true;
        }

        if (left_type->kind != right_type->kind) {
            return false;
        }

        switch (left_type->kind) {
        case Type::Kind::Type::Integer: {
            const IntegerType* left_integer = left_type->as<IntegerType>();
            const IntegerType* right_integer = right_type->as<IntegerType>();
            return left_integer->is_unsigned == right_integer->is_unsigned &&
                   left_integer->size == right_integer->size;
        }

        case Type::Kind::Type::Float: {
            const FloatType* left_float = left_type->as<FloatType>();
            const FloatType* right_float = right_type->as<FloatType>();
            return left_float->size == right_float->size;
        }

        case Type::Kind::Type::Pointer: {
            const PointerType* left_pointer = left_type->as<PointerType>();
            const PointerType* right_pointer = right_type->as<PointerType>();

            const Type* left_element = types.get(left_pointer->element);
            const Type* right_element = types.get(right_pointer->element);
            if (left_element->kind == Type::Kind::Type::Void ||
                right_element->kind == Type::Kind::Type::Void) {
                return true;
            }

            if (left_pointer->is_mutable != right_pointer->is_mutable) {
                return false;
            }

            return compatible(left_pointer->element, right_pointer->element);
        }

        case Type::Kind::Type::Array: {
            const ArrayType* left_array = left_type->as<ArrayType>();
            const ArrayType* right_array = right_type->as<ArrayType>();
            if (left_array->size != right_array->size) {
                return false;
            }
            return compatible(left_array->element, right_array->element);
        }

        case Type::Kind::Type::Struct: {
            const StructType* left_struct = left_type->as<StructType>();
            const StructType* right_struct = right_type->as<StructType>();
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
                if (!compatible(left_struct->fields[i].type, right_struct->fields[i].type)) {
                    return false;
                }
            }
            return true;
        }

        case Type::Kind::Type::Function: {
            const FunctionType* left_function = left_type->as<FunctionType>();
            const FunctionType* right_function = right_type->as<FunctionType>();
            if (!compatible(left_function->return_type, right_function->return_type)) {
                return false;
            }
            if (left_function->parameters.size() != right_function->parameters.size()) {
                return false;
            }
            for (std::size_t i = 0; i < left_function->parameters.size(); ++i) {
                if (!compatible(left_function->parameters[i].type,
                                right_function->parameters[i].type)) {
                    return false;
                }
            }
            return true;
        }

        default:
            return true;
        }
    }

    bool conflicts_with(const FunctionType& left, const FunctionType& right) const {
        if (left.parameters.size() != right.parameters.size()) {
            return false;
        }

        for (std::size_t i = 0; i < left.parameters.size(); ++i) {
            if (!compatible(left.parameters[i].type, right.parameters[i].type)) {
                return false;
            }
        }

        return true;
    }

    std::string type_to_string(TypeId id) const {
        const Type* type = types.get(id);
        if (type == nullptr) {
            return "?";
        }

        switch (type->kind) {
        case Type::Kind::Type::Any:
            return "any";
        case Type::Kind::Type::Unknown:
            return "unknown";
        case Type::Kind::Type::Void:
            return "void";
        case Type::Kind::Type::String:
            return "string";
        case Type::Kind::Type::Boolean:
            return "bool";
        case Type::Kind::Type::Integer: {
            const IntegerType* integer = type->as<IntegerType>();
            return (integer->is_unsigned ? "u" : "i") + std::to_string(integer->size);
        }
        case Type::Kind::Type::Float: {
            const FloatType* float_type = type->as<FloatType>();
            return "f" + std::to_string(float_type->size);
        }
        case Type::Kind::Type::Named: {
            const NamedType* named = type->as<NamedType>();
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
                    result += type_to_string(arg.type);
                }
                result += "]";
            }
            return result;
        }
        case Type::Kind::Type::Array: {
            const ArrayType* array = type->as<ArrayType>();
            std::string inner = type_to_string(array->element);
            inner += "[";
            if (array->size.has_value()) {
                inner += std::to_string(*array->size);
            }
            inner += "]";
            return inner;
        }
        case Type::Kind::Type::Pointer: {
            const PointerType* pointer = type->as<PointerType>();
            return pointer->is_mutable ? "*mut " + type_to_string(pointer->element)
                                       : "*" + type_to_string(pointer->element);
        }
        case Type::Kind::Type::Struct: {
            const StructType* struct_type = type->as<StructType>();
            return struct_type->name;
        }
        case Type::Kind::Type::Function: {
            const FunctionType* function_type = type->as<FunctionType>();
            return "function( -> " + type_to_string(function_type->return_type) + ")";
        }

        default:
            return "?";
        }
    }
};
