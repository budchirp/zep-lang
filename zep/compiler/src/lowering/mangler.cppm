module;

#include <cstdint>
#include <memory>
#include <optional>
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
import zep.frontend.sema.types.errors;
import zep.frontend.sema.scopes.scope;
export class Mangler {
  private:
    static bool has_linker_name(const FunctionSymbol* symbol) {
        return symbol != nullptr &&
            (symbol->has_attribute("name") || symbol->has_attribute(".test_name"));
    }

    static std::string encode_arguments(std::string result, const std::vector<GenericArgumentType>& arguments) {
        if (arguments.empty()) {
            return result;
        }

        result += "G" + std::to_string(arguments.size()) + "_";

        for (const auto& argument : arguments) {
            const auto encoded = encode(argument.binding());

            if (!encoded.has_value()) {
                return {};
            }

            result += std::to_string(encoded->size()) + "_" + *encoded;
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
            const auto element = encode(pointer->element);

            return element.empty() ? std::string() : result + element;
        }
        case Type::Kind::Type::Array: {
            const auto* array = type->as<ArrayType>();
            const auto element = encode(array->element);

            if (element.empty()) {
                return {};
            }

            if (const auto* concrete = std::get_if<ConcreteArrayExtent>(&array->extent);
                concrete != nullptr) {
                return "A" + std::to_string(concrete->value) + element;
            }

            return "a" + element;
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
            const auto name = Scope::qualified_name(*nominal);
            return encode_arguments("S" + std::to_string(name.size()) + name, nominal->generic_arguments);
        }
        case Type::Kind::Type::Function: {
            const auto* function = type->as<FunctionType>();
            const auto result_type = encode(function->return_type);

            if (result_type.empty()) {
                return {};
            }

            auto result = "F" + std::to_string(function->parameters.size()) + "_" +
                (function->variadic ? "V" : "N") + std::to_string(result_type.size()) + "_" + result_type;

            for (const auto& parameter : function->parameters) {
                const auto encoded = encode(parameter.type);

                if (encoded.empty()) {
                    return {};
                }

                result += std::to_string(encoded.size()) + "_" + encoded;
            }

            if (!function->errors.empty()) {
                result += "X" + std::to_string(function->errors.size()) + "_";

                for (const auto* error : function->errors) {
                    const auto encoded = ErrorSet::key(error);
                    result += std::to_string(encoded.size()) + "_" + encoded;
                }
            }

            return result;
        }
        default:
            return type->label;
        }
    }

    static std::optional<std::string> encode(const GenericBinding& binding) {
        if (const auto* type = std::get_if<TypeBinding>(&binding); type != nullptr) {
            auto encoded = encode(type->type);

            return encoded.empty() ? std::nullopt : std::optional<std::string>(std::move(encoded));
        }

        const auto& value = std::get<ConstBinding>(binding).value;
        if (!value.has_value()) {
            return std::nullopt;
        }

        return "c" + encode(value->type) + "_" + value->stable_encoding();
    }

    static std::optional<std::string> mangle(const std::string& name,
                                            const std::vector<GenericBinding>& arguments) {
        if (arguments.empty()) {
            return name;
        }

        std::string result = name + "$";

        for (std::size_t index = 0; index < arguments.size(); ++index) {
            if (index > 0) {
                result += "_";
            }

            const auto encoded = encode(arguments[index]);

            if (!encoded.has_value() || encoded->empty()) {
                return std::nullopt;
            }

            result += *encoded;
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

            const auto encoded = encode(parameters[index].type);

            if (encoded.empty()) {
                return {};
            }

            result += encoded;
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

        if (const auto* attribute = symbol->find_attribute(".test_name");
            attribute != nullptr && !attribute->arguments.empty()) {
            return attribute->arguments.front();
        }

        return AttributeResolver::get_linker_name(original_name, symbol->attributes);
    }

    static Linkage::Type function_linkage(const FunctionSymbol* symbol) {
        if (!is_mangled(symbol) || has_linker_name(symbol) ||
            (symbol != nullptr && symbol->visibility == Visibility::Type::Public)) {
            return Linkage::Type::External;
        }

        return symbol != nullptr && !symbol->module.empty()
            ? Linkage::Type::Hidden : Linkage::Type::Internal;
    }

    static std::string function_name(const std::string& original_name, const FunctionSymbol* symbol,
                                     const FunctionType* function_type) {
        auto name = linker_name(original_name, symbol);

        if (function_type != nullptr && original_name != "main" &&
            is_mangled(symbol) && !has_linker_name(symbol)) {
            if (symbol != nullptr && !symbol->module.empty()) {
                name = std::to_string(symbol->module.size()) + "_" + symbol->module + "::" + name;
            }

            name = mangle_parameters(name, function_type->parameters);
        }

        return name;
    }
};
