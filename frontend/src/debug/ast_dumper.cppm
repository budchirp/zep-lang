module;

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module zep.frontend.debug.ast_dumper;

import zep.common.logger;
import zep.frontend.ast;
import zep.frontend.ast.program;
import zep.frontend.sema.type;
import zep.frontend.sema.type.type_id;
import zep.frontend.arena.type;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;
import zep.frontend.sema.env;
import zep.frontend.sema.kind;
export class AstDumper : public Visitor<void> {
  private:
    TypeArena& types;

    int depth;
    bool with_indent;
    bool trailing_newline;

    void visit_child(Node& node, int new_depth, bool new_indent, bool new_newline) {
        int saved_depth = depth;
        bool saved_indent = with_indent;
        bool saved_newline = trailing_newline;

        depth = new_depth;
        with_indent = new_indent;
        trailing_newline = new_newline;
        visit_node(node);

        depth = saved_depth;
        with_indent = saved_indent;
        trailing_newline = saved_newline;
    }

  public:
    explicit AstDumper(TypeArena& types, int depth = 0, bool with_indent = true,
                       bool trailing_newline = true)
        : types(types), depth(depth), with_indent(with_indent), trailing_newline(trailing_newline) {
    }

    void dump_program(Program& program) {
        constexpr int depth = 0;

        Logger::print_indent(depth);
        Logger::print("Program(statements: [");

        if (program.statements.empty()) {
            Logger::print("])\n");
        } else {
            Logger::print("\n");

            for (std::size_t i = 0; i < program.statements.size(); ++i) {
                visit_child(*program.statements[i], depth + 1, true, false);
                Logger::print((i + 1 < program.statements.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth);
            Logger::print("])\n");
        }
    }

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
        if (generic_parameter_type.constraint.is_valid()) {
            dump_type(types, generic_parameter_type.constraint, depth + 1, false, false);
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
        if (generic_argument_type.type.is_valid()) {
            dump_type(types, generic_argument_type.type, depth + 1, false, false);
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
        if (parameter_type.type.is_valid()) {
            dump_type(types, parameter_type.type, depth + 1, false, false);
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
        if (struct_field_type.type.is_valid()) {
            dump_type(types, struct_field_type.type, depth + 1, false, false);
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

    void dump_type(TypeArena& arena, TypeId id, int depth, bool with_indent = true,
                   bool trailing_newline = true) {
        if (!id.is_valid()) {
            if (with_indent) {
                Logger::print_indent(depth);
            }
            Logger::print("null");
            if (trailing_newline) {
                Logger::print("\n");
            }
            return;
        }

        const Type* type = arena.get(id);
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

        case Type::Kind::Type::Unknown: {
            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("UnknownType()");

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
            const IntegerType* integer_type = type->as<IntegerType>();

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
            const FloatType* float_type = type->as<FloatType>();

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
            const NamedType* named_type = type->as<NamedType>();

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
                    dump_generic_argument_type(named_type->generic_arguments[i], depth + 2, true,
                                               false);
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
            const ArrayType* array_type = type->as<ArrayType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("ArrayType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("element: ");
            dump_type(arena, array_type->element, depth + 1, false, false);
            Logger::print(",\n");

            Logger::print_indent(depth + 1);
            Logger::print(
                "size: ",
                (array_type->size.has_value() ? std::to_string(*array_type->size) : "null"), "\n");

            Logger::print_indent(depth);
            Logger::print(")");

            if (trailing_newline) {
                Logger::print("\n");
            }
            break;
        }

        case Type::Kind::Type::Pointer: {
            const PointerType* pointer_type = type->as<PointerType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("PointerType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("is_mutable: ", (pointer_type->is_mutable ? "true" : "false"), ",\n");

            Logger::print_indent(depth + 1);
            Logger::print("element: ");
            dump_type(arena, pointer_type->element, depth + 1, false, false);
            Logger::print("\n");

            Logger::print_indent(depth);
            Logger::print(")");

            if (trailing_newline) {
                Logger::print("\n");
            }
            break;
        }

        case Type::Kind::Type::Struct: {
            const StructType* struct_type = type->as<StructType>();

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
                    dump_generic_parameter_type(struct_type->generic_parameters[i], depth + 2, true,
                                                false);
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
            const FunctionType* function_type = type->as<FunctionType>();

            if (with_indent) {
                Logger::print_indent(depth);
            }

            Logger::print("FunctionType(\n");

            Logger::print_indent(depth + 1);
            Logger::print("name: \"", function_type->name, "\",\n");

            Logger::print_indent(depth + 1);
            Logger::print("return_type: ");
            if (function_type->return_type.is_valid()) {
                dump_type(arena, function_type->return_type, depth + 1, false, false);
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
                    dump_generic_parameter_type(function_type->generic_parameters[i], depth + 2,
                                                true, false);
                    Logger::print(
                        (i + 1 < function_type->generic_parameters.size() ? ",\n" : "\n"));
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

    void dump_scope(Scope& scope, int depth, bool with_indent = true,
                    bool trailing_newline = true) {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("Scope(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", scope.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("types: [");
        if (scope.local_types_empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (const auto& [name, type_id] : scope.list_local_types()) {
                Logger::print_indent(depth + 2);
                Logger::print("ParameterType(\n");
                Logger::print_indent(depth + 3);
                Logger::print("name: \"", name, "\",\n");
                Logger::print_indent(depth + 3);
                Logger::print("type: ");
                dump_type(types, type_id, depth + 3, false, false);
                Logger::print("\n");
                Logger::print_indent(depth + 2);
                Logger::print(")\n");
            }
            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("vars: [");
        if (scope.local_vars_empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (const auto& [name, symbol] : scope.list_local_vars()) {
                Logger::print_indent(depth + 2);
                Logger::print("VarSymbol(\n");
                Logger::print_indent(depth + 3);
                Logger::print("name: \"", symbol->name, "\",\n");
                Logger::print_indent(depth + 3);
                Logger::print("type: ");
                if (symbol->type.is_valid()) {
                    dump_type(types, symbol->type, depth + 3, false, false);
                } else {
                    Logger::print("null");
                }
                Logger::print(",\n");
                Logger::print_indent(depth + 3);
                Logger::print("storage_kind: ", StorageKind::to_string(symbol->storage_kind), "\n");
                Logger::print_indent(depth + 2);
                Logger::print(")\n");
            }
            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("functions: [");
        if (scope.local_functions_empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (const auto& [function_name, symbol] : scope.list_local_functions()) {
                Logger::print_indent(depth + 2);
                Logger::print("FunctionSymbol(\n");
                Logger::print_indent(depth + 3);
                Logger::print("name: \"", function_name, "\",\n");
                Logger::print_indent(depth + 3);
                Logger::print("type: ");
                if (symbol->type.is_valid()) {
                    dump_type(types, symbol->type, depth + 3, false, false);
                } else {
                    Logger::print("null");
                }
                Logger::print(",\n");
                Logger::print_indent(depth + 3);
                Logger::print("visibility: ", Visibility::to_string(symbol->visibility), "\n");
                Logger::print_indent(depth + 2);
                Logger::print(")\n");
            }
            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void dump_env(Env& env) {
        Logger::print_indent(0);
        Logger::print("Env(\n");

        Logger::print_indent(1);
        Logger::print("global_scope: ");
        dump_scope(*env.global_scope, 1, false, true);

        Logger::print_indent(0);
        Logger::print(")\n");
    }

    void visit(TypeExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("TypeExpression(type: ");
        if (node.type.is_valid()) {
            dump_type(types, node.type, depth, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(GenericParameter& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("GenericParameter(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("constraint: ");
        if (node.constraint != nullptr) {
            visit_child(*node.constraint, depth + 1, false, false);
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

    void visit(GenericArgument& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("GenericArgument(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        if (node.type != nullptr) {
            visit_child(*node.type, depth + 1, false, false);
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

    void visit(Parameter& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("Parameter(\n");

        Logger::print_indent(depth + 1);
        Logger::print("is_variadic: ", (node.is_variadic ? "true" : "false"), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        if (node.type != nullptr) {
            visit_child(*node.type, depth + 1, false, false);
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

    void visit(Argument& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("Argument(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        visit_child(*node.value, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(FunctionPrototype& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("FunctionPrototype(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("generic_parameters: [");
        if (node.generic_parameters.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.generic_parameters.size(); ++i) {
                visit_child(*node.generic_parameters[i], depth + 2, true, false);
                Logger::print((i + 1 < node.generic_parameters.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("parameters: [");
        if (node.parameters.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.parameters.size(); ++i) {
                visit_child(*node.parameters[i], depth + 2, true, false);
                Logger::print((i + 1 < node.parameters.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("return_type: ");
        if (node.return_type != nullptr) {
            visit_child(*node.return_type, depth + 1, false, false);
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

    void visit(StructField& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("StructField(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(node.visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        if (node.type != nullptr) {
            visit_child(*node.type, depth + 1, false, false);
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

    void visit(StructLiteralField& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("StructLiteralField(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        visit_child(*node.value, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(NumberLiteral& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("NumberLiteral(value: \"", node.value, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(FloatLiteral& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("FloatLiteral(value: \"", node.value, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(StringLiteral& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("StringLiteral(value: \"", node.value, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(BooleanLiteral& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("BooleanLiteral(value: ", (node.value ? "true" : "false"), ")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(IdentifierExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("IdentifierExpression(name: \"", node.name, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(BinaryExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("BinaryExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("left: ");
        visit_child(*node.left, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("op: ", BinaryExpression::Operator::to_string(node.op), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("right: ");
        visit_child(*node.right, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(UnaryExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("UnaryExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("op: ", UnaryExpression::Operator::to_string(node.op), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("operand: ");
        visit_child(*node.operand, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(CallExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("CallExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("callee: ");
        visit_child(*node.callee, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("generic_arguments: [");
        if (node.generic_arguments.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.generic_arguments.size(); ++i) {
                visit_child(*node.generic_arguments[i], depth + 2, true, false);
                Logger::print((i + 1 < node.generic_arguments.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("arguments: [");
        if (node.arguments.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.arguments.size(); ++i) {
                visit_child(*node.arguments[i], depth + 2, true, false);
                Logger::print((i + 1 < node.arguments.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("]\n");
        }

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(IndexExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("IndexExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        visit_child(*node.value, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("index: ");
        visit_child(*node.index, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(MemberExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("MemberExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        visit_child(*node.value, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("member: \"", node.member, "\"\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(AssignExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("AssignExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("target: ");
        visit_child(*node.target, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        visit_child(*node.value, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(StructLiteralExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("StructLiteralExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: ");
        visit_child(*node.name, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("generic_arguments: [");
        if (node.generic_arguments.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.generic_arguments.size(); ++i) {
                visit_child(*node.generic_arguments[i], depth + 2, true, false);
                Logger::print((i + 1 < node.generic_arguments.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("fields: [");
        if (node.fields.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.fields.size(); ++i) {
                visit_child(*node.fields[i], depth + 2, true, false);
                Logger::print((i + 1 < node.fields.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("]\n");
        }

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(BlockStatement& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("BlockStatement(statements: [");
        if (node.statements.empty()) {
            Logger::print("])");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.statements.size(); ++i) {
                visit_child(*node.statements[i], depth + 1, true, false);
                Logger::print((i + 1 < node.statements.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth);
            Logger::print("])");
        }

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(ExpressionStatement& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ExpressionStatement(expression: ");
        visit_child(*node.expression, depth, false, false);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(IfExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("IfStatement(\n");

        Logger::print_indent(depth + 1);
        Logger::print("condition: ");
        visit_child(*node.condition, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("then_branch: ");
        visit_child(*node.then_branch, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("else_branch: ");
        if (node.else_branch != nullptr) {
            visit_child(*node.else_branch, depth + 1, false, false);
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

    void visit(ReturnStatement& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ReturnStatement(value: ");
        if (node.value != nullptr) {
            visit_child(*node.value, depth, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(StructDeclaration& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("StructDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(node.visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("generic_parameters: [");
        if (node.generic_parameters.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.generic_parameters.size(); ++i) {
                visit_child(*node.generic_parameters[i], depth + 2, true, false);
                Logger::print((i + 1 < node.generic_parameters.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("fields: [");
        if (node.fields.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.fields.size(); ++i) {
                visit_child(*node.fields[i], depth + 2, true, false);
                Logger::print((i + 1 < node.fields.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("]\n");
        }

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(VarDeclaration& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("VarDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(node.visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("storage_kind: ", StorageKind::to_string(node.storage_kind), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        if (node.annotation != nullptr) {
            visit_child(*node.annotation, depth + 1, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("initializer: ");
        if (node.initializer != nullptr) {
            visit_child(*node.initializer, depth + 1, false, false);
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

    void visit(FunctionDeclaration& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("FunctionDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(node.visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("prototype: ");
        visit_child(*node.prototype, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("body: ");
        visit_child(*node.body, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(ExternFunctionDeclaration& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ExternFunctionDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(node.visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("prototype: ");
        visit_child(*node.prototype, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(ExternVarDeclaration& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ExternVarDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(node.visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        if (node.annotation != nullptr) {
            visit_child(*node.annotation, depth + 1, false, false);
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

    void visit(ImportStatement& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ImportStatement(path: [");
        if (node.path.empty()) {
            Logger::print("])");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.path.size(); ++i) {
                visit_child(*node.path[i], depth + 1, true, false);
                Logger::print((i + 1 < node.path.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth);
            Logger::print("])");
        }

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};
