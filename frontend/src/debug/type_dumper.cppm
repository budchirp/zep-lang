module;

#include <string>
#include <variant>
#include <vector>

export module zep.frontend.debug.type_dumper;

import zep.common.logger;
import zep.frontend.sema.type;
import zep.frontend.sema.kind;

export class TypeDumper {
  public:
    void dump_generic_parameter(const GenericParameterType& param, int depth,
                                bool with_indent = true, bool trailing_newline = true) {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("GenericParameterType(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", param.name, "\",\n");

        Logger::print_indent(depth + 1);
        if (param.is_const()) {
            Logger::print("value_type: ");
        } else {
            Logger::print("constraint: ");
        }

        if (param.type != nullptr) {
            dump(param.type, depth + 1, false, false);
        } else {
            Logger::print("null");
        }

        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void dump_generic_argument(const GenericArgumentType& arg, int depth, bool with_indent = true,
                               bool trailing_newline = true) {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("GenericArgumentType(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", arg.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");

        if (arg.is_const()) {
            Logger::print(arg.const_binding->to_string());
        } else if (arg.type != nullptr) {
            dump(arg.type, depth + 1, false, false);
        } else {
            Logger::print("null");
        }

        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void dump_parameter(const ParameterType& param, int depth, bool with_indent = true,
                        bool trailing_newline = true) {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ParameterType(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", param.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");

        if (param.type != nullptr) {
            dump(param.type, depth + 1, false, false);
        } else {
            Logger::print("null");
        }

        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void dump_field(const FieldType& field, int depth, bool with_indent = true,
                    bool trailing_newline = true) {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("FieldType(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", field.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");

        if (field.type != nullptr) {
            dump(field.type, depth + 1, false, false);
        } else {
            Logger::print("null");
        }

        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void dump(const Type* type, int depth, bool with_indent = true, bool trailing_newline = true) {
        if (type == nullptr) {
            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("null");

            if (trailing_newline) {
                Logger::print("\n");
            }

            return;
        }

        switch (type->kind) {
        case Type::Kind::Type::Any: {
            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("AnyType()");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Void: {
            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("VoidType()");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Never: {
            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("NeverType()");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::String: {
            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("StringType()");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Boolean: {
            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("BooleanType()");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Char: {
            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("CharType()");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Integer: {
            const auto* integer = type->as<IntegerType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("IntegerType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("is_unsigned: ", (integer->is_unsigned ? "true" : "false"), ",\n");

            Logger::print_indent(depth + 1);
            Logger::print("size: ", integer->size, "\n");

            Logger::print_indent(depth);
            Logger::print(")");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Float: {
            const auto* floating = type->as<FloatType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("FloatType(size: ", floating->size, ")");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Named: {
            const auto* named = type->as<NamedType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("NamedType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("name: \"", named->name, "\",\n");

            Logger::print_indent(depth + 1);
            Logger::print("generic_arguments: [");

            if (named->generic_arguments.empty()) {
                Logger::print("]\n");
            } else {
                Logger::print("\n");

                for (std::size_t i = 0; i < named->generic_arguments.size(); ++i) {
                    dump_generic_argument(named->generic_arguments[i], depth + 2, true, false);
                    Logger::print((i + 1 < named->generic_arguments.size() ? ",\n" : "\n"));
                }

                Logger::print_indent(depth + 1);
                Logger::print("]\n");
            }

            Logger::print_indent(depth);
            Logger::print(")");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Array: {
            const auto* array = type->as<ArrayType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("ArrayType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("element: ");
            dump(array->element, depth + 1, false, false);
            Logger::print(",\n");

            Logger::print_indent(depth + 1);
            if (const auto* concrete = std::get_if<ConcreteArrayExtent>(&array->extent);
                concrete != nullptr) {
                Logger::print("size: ", std::to_string(concrete->value), "\n");
            } else if (std::holds_alternative<DependentArrayExtent>(array->extent)) {
                Logger::print("size: <dependent>\n");
            } else {
                Logger::print("size: null\n");
            }

            Logger::print_indent(depth);
            Logger::print(")");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Pointer: {
            const auto* pointer = type->as<PointerType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("PointerType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("is_mutable: ", (pointer->is_mutable ? "true" : "false"), ",\n");

            Logger::print_indent(depth + 1);
            Logger::print("element: ");
            dump(pointer->element, depth + 1, false, false);
            Logger::print("\n");

            Logger::print_indent(depth);
            Logger::print(")");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Struct: {
            const auto* structure = type->as<StructType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("StructType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("name: \"", structure->name, "\",\n");

            Logger::print_indent(depth + 1);
            Logger::print("generic_parameters: [");

            if (structure->generic_parameters.empty()) {
                Logger::print("],\n");
            } else {
                Logger::print("\n");

                for (std::size_t i = 0; i < structure->generic_parameters.size(); ++i) {
                    dump_generic_parameter(structure->generic_parameters[i], depth + 2, true,
                                           false);
                    Logger::print((i + 1 < structure->generic_parameters.size() ? ",\n" : "\n"));
                }

                Logger::print_indent(depth + 1);
                Logger::print("],\n");
            }

            Logger::print_indent(depth + 1);
            Logger::print("fields: [");

            if (structure->fields.empty()) {
                Logger::print("]\n");
            } else {
                Logger::print("\n");

                for (std::size_t i = 0; i < structure->fields.size(); ++i) {
                    dump_field(structure->fields[i], depth + 2, true, false);
                    Logger::print((i + 1 < structure->fields.size() ? ",\n" : "\n"));
                }

                Logger::print_indent(depth + 1);
                Logger::print("]\n");
            }

            Logger::print_indent(depth);
            Logger::print(")");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Enum: {
            const auto* enumeration = type->as<EnumType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("EnumType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("name: \"", enumeration->name, "\",\n");

            Logger::print_indent(depth + 1);
            Logger::print("generic_parameters: [");

            if (enumeration->generic_parameters.empty()) {
                Logger::print("],\n");
            } else {
                Logger::print("\n");

                for (std::size_t i = 0; i < enumeration->generic_parameters.size(); ++i) {
                    dump_generic_parameter(enumeration->generic_parameters[i], depth + 2, true,
                                           false);
                    Logger::print((i + 1 < enumeration->generic_parameters.size() ? ",\n" : "\n"));
                }

                Logger::print_indent(depth + 1);
                Logger::print("],\n");
            }

            Logger::print_indent(depth + 1);
            Logger::print("variants: [");

            if (enumeration->variants.empty()) {
                Logger::print("]\n");
            } else {
                Logger::print("\n");

                for (std::size_t i = 0; i < enumeration->variants.size(); ++i) {
                    const auto& variant = enumeration->variants[i];

                    Logger::print_indent(depth + 2);
                    Logger::print("EnumVariantType(\n");

                    Logger::print_indent(depth + 3);
                    Logger::print("name: \"", variant.name, "\",\n");

                    Logger::print_indent(depth + 3);
                    Logger::print("index: ", variant.index, ",\n");

                    Logger::print_indent(depth + 3);
                    Logger::print("fields: [");
                    if (variant.fields.empty()) {
                        Logger::print("]\n");
                    } else {
                        Logger::print("\n");
                        for (std::size_t j = 0; j < variant.fields.size(); ++j) {
                            dump_field(variant.fields[j], depth + 4, true, false);
                            Logger::print((j + 1 < variant.fields.size() ? ",\n" : "\n"));
                        }
                        Logger::print_indent(depth + 3);
                        Logger::print("]\n");
                    }

                    Logger::print_indent(depth + 2);
                    Logger::print(")");
                    Logger::print((i + 1 < enumeration->variants.size() ? ",\n" : "\n"));
                }

                Logger::print_indent(depth + 1);
                Logger::print("]\n");
            }

            Logger::print_indent(depth);
            Logger::print(")");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Interface: {
            const auto* interface_type = type->as<InterfaceType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("InterfaceType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("name: \"", interface_type->name, "\",\n");

            Logger::print_indent(depth + 1);
            Logger::print("methods: ", interface_type->methods.size(), "\n");

            Logger::print_indent(depth);
            Logger::print(")");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Function: {
            const auto* function = type->as<FunctionType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("FunctionType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("name: \"", function->name, "\",\n");

            Logger::print_indent(depth + 1);
            Logger::print("return_type: ");

            if (function->return_type != nullptr) {
                dump(function->return_type, depth + 1, false, false);
            } else {
                Logger::print("null");
            }

            Logger::print(",\n");

            Logger::print_indent(depth + 1);
            Logger::print("parameters: [");

            if (function->parameters.empty()) {
                Logger::print("],\n");
            } else {
                Logger::print("\n");

                for (std::size_t i = 0; i < function->parameters.size(); ++i) {
                    dump_parameter(function->parameters[i], depth + 2, true, false);
                    Logger::print((i + 1 < function->parameters.size() ? ",\n" : "\n"));
                }

                Logger::print_indent(depth + 1);
                Logger::print("],\n");
            }

            Logger::print_indent(depth + 1);
            Logger::print("generics: [");

            if (function->generic_parameters.empty()) {
                Logger::print("],\n");
            } else {
                Logger::print("\n");

                for (std::size_t i = 0; i < function->generic_parameters.size(); ++i) {
                    dump_generic_parameter(function->generic_parameters[i], depth + 2, true, false);
                    Logger::print((i + 1 < function->generic_parameters.size() ? ",\n" : "\n"));
                }

                Logger::print_indent(depth + 1);
                Logger::print("],\n");
            }

            Logger::print_indent(depth + 1);
            Logger::print("variadic: ", (function->variadic ? "true" : "false"), "\n");

            Logger::print_indent(depth);
            Logger::print(")");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }
        }
    }
};
