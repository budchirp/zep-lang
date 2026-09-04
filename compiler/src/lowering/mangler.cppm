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

import zep.frontend.sema.kind;
import zep.frontend.sema.resolver.attribute;
import zep.frontend.sema.scope;
import zep.frontend.sema.type;
export class Mangler {
  private:
    static std::string encode_string_value(const std::string& value) {
        auto result = std::to_string(value.size()) + "_";

        for (const auto ch : value) {
            result += std::to_string(static_cast<unsigned>(static_cast<unsigned char>(ch))) + "_";
        }

        return result;
    }

    static std::string encode_signed_integer(auto value) {
        auto result = std::to_string(value);
        if (!result.empty() && result.front() == '-') {
            result.front() = 'n';
            return result;
        }

        return "p" + result;
    }

    static std::string encode_compile_time_value(const CompileTimeValue& value) {
        switch (value.kind) {
        case CompileTimeValue::Kind::Type::SignedInteger:
            return "s" + encode_signed_integer(std::get<std::int64_t>(value.payload));

        case CompileTimeValue::Kind::Type::UnsignedInteger:
            return "u" + std::to_string(std::get<std::uint64_t>(value.payload));

        case CompileTimeValue::Kind::Type::Float:
            return "f" + std::to_string(std::get<std::uint64_t>(value.payload));

        case CompileTimeValue::Kind::Type::Boolean:
            return std::get<bool>(value.payload) ? "b1" : "b0";

        case CompileTimeValue::Kind::Type::Char:
            return "h" + std::to_string(std::get<std::uint8_t>(value.payload));

        case CompileTimeValue::Kind::Type::String:
            return "t" + encode_string_value(std::get<std::string>(value.payload));

        case CompileTimeValue::Kind::Type::Null:
            return "null";

        case CompileTimeValue::Kind::Type::Struct:
        case CompileTimeValue::Kind::Type::Array: {
            const auto& elements = std::get<std::vector<CompileTimeValue>>(value.payload);
            auto result = value.kind == CompileTimeValue::Kind::Type::Struct ? std::string("s")
                                                                             : std::string("a");
            result += std::to_string(elements.size()) + "_";
            for (const auto& element : elements) {
                const auto encoded = encode(element.type) + encode_compile_time_value(element);
                result += std::to_string(encoded.size()) + "_" + encoded;
            }
            return result;
        }
        }

        return {};
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
            std::string result = "N" + std::to_string(named->name.size()) + named->name;
            for (const auto& arg : named->generic_arguments) {
                if (arg.is_const()) {
                    result += encode(arg.binding());
                } else {
                    result += encode(arg.type);
                }
            }
            return result;
        }
        case Type::Kind::Type::Struct:
        case Type::Kind::Type::Enum:
        case Type::Kind::Type::Interface: {
            const auto* nominal = type->as_nominal();
            if (nominal == nullptr) {
                return type->label;
            }
            std::string result = "S" + std::to_string(nominal->name.size()) + nominal->name;
            for (const auto& arg : nominal->generic_arguments) {
                if (arg.is_const()) {
                    result += encode(arg.binding());
                } else {
                    result += encode(arg.type);
                }
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

        return "c" + encode(value->type) + "_" + encode_compile_time_value(*value);
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
        return symbol != nullptr && symbol->is_mangled();
    }

    template <typename SymbolType>
    static std::string linker_name(const std::string& original_name, const SymbolType* symbol) {
        if (symbol == nullptr) {
            return original_name;
        }

        return AttributeResolver::get_linker_name(original_name, symbol->attributes);
    }

    static bool should_mangle(const std::string& original_name, bool mangled,
                              bool has_custom_name) {
        return original_name != "main" && mangled && !has_custom_name;
    }

    static std::string resolve_function_name(const std::string& original_name,
                                             const FunctionSymbol* symbol,
                                             const std::vector<ParameterType>& parameters) {
        auto name = linker_name(original_name, symbol);
        auto has_custom_name = name != original_name;
        auto mangled = is_mangled(symbol);

        if (should_mangle(original_name, mangled, has_custom_name)) {
            name = Mangler::mangle_parameters(name, parameters);
        }

        return name;
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
        if (function_type == nullptr) {
            return linker_name(original_name, symbol);
        }

        return resolve_function_name(original_name, symbol, function_type->parameters);
    }

    static std::string identifier_name(const std::string& original_name,
                                       const FunctionType* function_type,
                                       const FunctionSymbol* symbol,
                                       const std::vector<FunctionSymbol*>& overloads) {
        auto name = linker_name(original_name, symbol);
        auto has_custom_name = name != original_name;

        auto mangled = false;
        if (overloads.size() > 1) {
            mangled = true;
        } else if (symbol != nullptr) {
            mangled = symbol->is_mangled();
        } else if (overloads.size() == 1) {
            mangled = overloads[0]->is_mangled();
        }

        if (function_type != nullptr && should_mangle(original_name, mangled, has_custom_name)) {
            name = Mangler::mangle_parameters(name, function_type->parameters);
        }

        return name;
    }
};
