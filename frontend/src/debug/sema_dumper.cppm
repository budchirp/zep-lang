module;

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

export module zep.frontend.debug.sema_dumper;

import zep.common.logger;
import zep.frontend.sema.type;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;
import zep.frontend.sema.kind;

export class SemaDumper {
  private:
    int depth;

  public:
    explicit SemaDumper(int depth = 0) : depth(depth) {}

    void dump_generic_parameter_type(const GenericParameterType& generic_parameter_type, int depth,
                                     bool with_indent = true, bool trailing_newline = true) {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("GenericParameterType(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", generic_parameter_type.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("constraint: ");

        if (generic_parameter_type.constraint != nullptr) {
            dump_type(generic_parameter_type.constraint, depth + 1, false, false);
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

    void dump_generic_argument_type(const GenericArgumentType& generic_argument_type, int depth,
                                    bool with_indent = true, bool trailing_newline = true) {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("GenericArgumentType(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", generic_argument_type.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");

        if (generic_argument_type.type != nullptr) {
            dump_type(generic_argument_type.type, depth + 1, false, false);
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

    void dump_parameter_type(const ParameterType& parameter_type, int depth,
                             bool with_indent = true, bool trailing_newline = true) {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ParameterType(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", parameter_type.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");

        if (parameter_type.type != nullptr) {
            dump_type(parameter_type.type, depth + 1, false, false);
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

    void dump_struct_field_type(const StructFieldType& struct_field_type, int depth,
                                bool with_indent = true, bool trailing_newline = true) {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("StructFieldType(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", struct_field_type.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");

        if (struct_field_type.type != nullptr) {
            dump_type(struct_field_type.type, depth + 1, false, false);
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

    void dump_type(const Type* type, int depth, bool with_indent = true,
                   bool trailing_newline = true) {
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

        case Type::Kind::Type::Integer: {
            const auto* integer_type = type->as<IntegerType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("IntegerType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("is_unsigned: ", (integer_type->is_unsigned ? "true" : "false"), ",\n");

            Logger::print_indent(depth + 1);
            Logger::print("size: ", integer_type->size, "\n");

            Logger::print_indent(depth);
            Logger::print(")");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Float: {
            const auto* float_type = type->as<FloatType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("FloatType(size: ", float_type->size, ")");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Named: {
            const auto* named_type = type->as<NamedType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("NamedType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("name: \"", named_type->name, "\",\n");

            Logger::print_indent(depth + 1);
            Logger::print("generic_arguments: [");

            if (named_type->generic_arguments.empty()) {
                Logger::print("]\n");
            } else {
                Logger::print("\n");

                for (std::size_t i = 0; i < named_type->generic_arguments.size(); ++i) {
                    dump_generic_argument_type(named_type->generic_arguments[i], depth + 2, true, false);
                    Logger::print((i + 1 < named_type->generic_arguments.size() ? ",\n" : "\n"));
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
            const auto* array_type = type->as<ArrayType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("ArrayType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("element: ");
            dump_type(array_type->element, depth + 1, false, false);
            Logger::print(",\n");

            Logger::print_indent(depth + 1);
            Logger::print("size: ", (array_type->size.has_value() ? std::to_string(*array_type->size) : "null"), "\n");

            Logger::print_indent(depth);
            Logger::print(")");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Pointer: {
            const auto* pointer_type = type->as<PointerType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("PointerType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("is_mutable: ", (pointer_type->is_mutable ? "true" : "false"), ",\n");

            Logger::print_indent(depth + 1);
            Logger::print("element: ");
            dump_type(pointer_type->element, depth + 1, false, false);
            Logger::print("\n");

            Logger::print_indent(depth);
            Logger::print(")");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }

        case Type::Kind::Type::Struct: {
            const auto* struct_type = type->as<StructType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("StructType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("name: \"", struct_type->name, "\",\n");

            Logger::print_indent(depth + 1);
            Logger::print("generic_parameters: [");

            if (struct_type->generic_parameters.empty()) {
                Logger::print("],\n");
            } else {
                Logger::print("\n");

                for (std::size_t i = 0; i < struct_type->generic_parameters.size(); ++i) {
                    dump_generic_parameter_type(struct_type->generic_parameters[i], depth + 2, true, false);
                    Logger::print((i + 1 < struct_type->generic_parameters.size() ? ",\n" : "\n"));
                }

                Logger::print_indent(depth + 1);
                Logger::print("],\n");
            }

            Logger::print_indent(depth + 1);
            Logger::print("fields: [");

            if (struct_type->fields.empty()) {
                Logger::print("]\n");
            } else {
                Logger::print("\n");

                for (std::size_t i = 0; i < struct_type->fields.size(); ++i) {
                    dump_struct_field_type(struct_type->fields[i], depth + 2, true, false);
                    Logger::print((i + 1 < struct_type->fields.size() ? ",\n" : "\n"));
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

        case Type::Kind::Type::Function: {
            const auto* function_type = type->as<FunctionType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("FunctionType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("name: \"", function_type->name, "\",\n");

            Logger::print_indent(depth + 1);
            Logger::print("return_type: ");

            if (function_type->return_type != nullptr) {
                dump_type(function_type->return_type, depth + 1, false, false);
            } else {
                Logger::print("null");
            }

            Logger::print(",\n");

            Logger::print_indent(depth + 1);
            Logger::print("parameters: [");

            if (function_type->parameters.empty()) {
                Logger::print("],\n");
            } else {
                Logger::print("\n");

                for (std::size_t i = 0; i < function_type->parameters.size(); ++i) {
                    dump_parameter_type(function_type->parameters[i], depth + 2, true, false);
                    Logger::print((i + 1 < function_type->parameters.size() ? ",\n" : "\n"));
                }

                Logger::print_indent(depth + 1);
                Logger::print("],\n");
            }

            Logger::print_indent(depth + 1);
            Logger::print("generics: [");

            if (function_type->generic_parameters.empty()) {
                Logger::print("],\n");
            } else {
                Logger::print("\n");

                for (std::size_t i = 0; i < function_type->generic_parameters.size(); ++i) {
                    dump_generic_parameter_type(function_type->generic_parameters[i], depth + 2, true, false);
                    Logger::print((i + 1 < function_type->generic_parameters.size() ? ",\n" : "\n"));
                }

                Logger::print_indent(depth + 1);
                Logger::print("],\n");
            }

            Logger::print_indent(depth + 1);
            Logger::print("variadic: ", (function_type->variadic ? "true" : "false"), "\n");

            Logger::print_indent(depth);
            Logger::print(")");

            if (trailing_newline) {
                Logger::print("\n");
            }

            break;
        }
        }
    }

    void dump_symbol(const Symbol* symbol, int depth, bool with_indent = true, bool trailing_newline = true) {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        auto kind_str = std::string();

        switch (symbol->kind) {
            case Symbol::Kind::Type::Var: kind_str = "VarSymbol"; break;
            case Symbol::Kind::Type::Function: kind_str = "FunctionSymbol"; break;
            case Symbol::Kind::Type::Type: kind_str = "TypeSymbol"; break;
        }

        Logger::print(kind_str, "(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", symbol->name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(symbol->visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(symbol->type, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void dump_scope(const Scope* scope, int depth = 0) {
        if (scope == nullptr) {
            return;
        }

        Logger::print_indent(depth);
        Logger::print("Scope(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", scope->name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("kind: ", Scope::Kind::to_string(scope->kind), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("symbols: [");

        auto all_symbols = std::vector<Symbol*>();

        for (auto& [name, symbol] : scope->types) {
            all_symbols.push_back(symbol);
        }

        for (auto& [name, symbol] : scope->variables) {
            all_symbols.push_back(symbol);
        }

        for (auto& [name, symbols] : scope->functions) {
            for (auto* symbol : symbols) {
                all_symbols.push_back(symbol);
            }
        }

        if (all_symbols.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");

            for (std::size_t i = 0; i < all_symbols.size(); ++i) {
                dump_symbol(all_symbols[i], depth + 2, true, false);
                Logger::print((i + 1 < all_symbols.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("]\n");
        }

        Logger::print_indent(depth);
        Logger::print(")\n");
    }
};
