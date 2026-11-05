module;

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

export module zep.compiler.lowering.mangler;

import zep.frontend.kind;
import zep.frontend.sema.declaration.attribute;
import zep.frontend.sema.symbols;
import zep.frontend.sema.types;
export class Mangler {
  private:
    static std::string encode_arguments(std::string result, const std::vector<GenericArgumentType>& arguments) {
        if (arguments.empty()) {
            return result;
        }

        result += "G" + std::to_string(arguments.size()) + "_";

        for (const auto& argument : arguments) {
            const auto encoded = encode(argument.binding());
            result += std::to_string(encoded.size()) + "_" + encoded;
        }

        return result + "E";
    }

  public:
    static std::string encode(const Type* type) {
        if (type == nullptr) {
            return "v";
        }

        switch (type->kind) {
        case Type::Kind::Type::Integer: {
            const auto* integer = type->as<IntegerType>();
            std::string prefix = integer->is_unsigned ? "u" : "i";
            return prefix + std::to_string(integer->size);
        }
        case Type::Kind::Type::Float: {
            const auto* float_type = type->as<FloatType>();
            return "f" + std::to_string(float_type->size);
        }
        case Type::Kind::Type::Pointer: {
            const auto* pointer = type->as<PointerType>();
            std::string result = pointer->is_mutable ? "p" : "P";
            if (pointer->reference_kind != PointerType::ReferenceKind::Type::None) {
                result = pointer->reference_kind == PointerType::ReferenceKind::Type::Move ? "M" : pointer->is_mutable ? "r" : "R";
            }
            return result + encode(pointer->element);
        }
        case Type::Kind::Type::Array: {
            const auto* array = type->as<ArrayType>();
            if (const auto* concrete = std::get_if<ConcreteArrayExtent>(&array->extent);
                concrete != nullptr) {
                return "A" + std::to_string(concrete->value) + encode(array->element);
            }
            return "a" + encode(array->element);
        }
        case Type::Kind::Type::Named: {
            const auto* named = type->as<NamedType>();
            return encode_arguments("N" + std::to_string(named->name.size()) + named->name,
                                    named->generic_arguments);
        }
        case Type::Kind::Type::Struct:
        case Type::Kind::Type::Enum:
        case Type::Kind::Type::Interface: {
            const auto* nominal = type->as_nominal();
            if (nominal == nullptr) {
                return type->label;
            }
            const auto& name = nominal->definition != nullptr ? nominal->definition->name : nominal->name;
            return encode_arguments("S" + std::to_string(name.size()) + name, nominal->generic_arguments);
        }
        case Type::Kind::Type::Function: {
            const auto* function = type->as<FunctionType>();
            const auto result_type = encode(function->return_type);
            auto result = "F" + std::to_string(function->parameters.size()) + "_" +
                (function->variadic ? "V" : "N") + std::to_string(result_type.size()) + "_" + result_type;

            for (const auto& parameter : function->parameters) {
                const auto encoded = encode(parameter.type);
                result += std::to_string(encoded.size()) + "_" + encoded;
            }

            return result;
        }
        default:
            return type->label;
        }
    }

    static std::string encode(const GenericBinding& binding) {
        if (const auto* type = std::get_if<TypeBinding>(&binding); type != nullptr) {
            return encode(type->type);
        }

        const auto& value = std::get<ConstBinding>(binding).value;
        if (!value.has_value()) {
            throw std::invalid_argument("cannot mangle a dependent const binding");
        }

        return "c" + encode(value->type) + "_" + value->stable_encoding();
    }

    static std::string mangle(const std::string& name,
                              const std::vector<GenericBinding>& arguments) {
        if (arguments.empty()) {
            return name;
        }

        std::string result = name + "$";

        for (std::size_t index = 0; index < arguments.size(); ++index) {
            if (index > 0) {
                result += "_";
            }

            result += encode(arguments[index]);
        }

        return result;
    }

    static std::string mangle_parameters(const std::string& name,
                                         const std::vector<ParameterType>& parameters) {
        if (parameters.empty()) {
            return name;
        }

        std::string result = name + "$";

        for (std::size_t index = 0; index < parameters.size(); ++index) {
            if (index > 0) {
                result += "_";
            }

            result += encode(parameters[index].type);
        }

        return result;
    }

    static bool is_mangled(const FunctionSymbol* symbol) {
        if (symbol == nullptr) {
            return false;
        }

        const auto* attribute = symbol->find_attribute("mangle");

        if (attribute == nullptr) {
            return symbol->linkage != Linkage::Type::External;
        }

        return attribute->arguments.empty() || attribute->arguments.front() != "false";
    }

    template <typename SymbolType>
    static std::string linker_name(const std::string& original_name, const SymbolType* symbol) {
        if (symbol == nullptr) {
            return original_name;
        }

        return AttributeResolver::get_linker_name(original_name, symbol->attributes);
    }

    static Linkage::Type function_linkage(const std::string& original_name,
                                          const FunctionSymbol* symbol) {
        auto name = linker_name(original_name, symbol);
        auto has_custom_name = name != original_name;
        auto mangled = is_mangled(symbol);
        auto is_public = symbol != nullptr && symbol->visibility == Visibility::Type::Public;

        return (!mangled || has_custom_name || is_public) ? Linkage::Type::External
                                                          : Linkage::Type::Internal;
    }

    static std::string function_name(const std::string& original_name, const FunctionSymbol* symbol,
                                     const FunctionType* function_type) {
        auto name = linker_name(original_name, symbol);

        if (function_type != nullptr && original_name != "main" &&
            is_mangled(symbol) && name == original_name) {
            name = mangle_parameters(name, function_type->parameters);
        }

        return name;
    }
};
