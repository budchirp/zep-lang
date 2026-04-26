module;

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module zep.frontend.debug.ast_dumper;

import zep.common.logger;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.sema.type;
import zep.frontend.arena;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;
import zep.frontend.sema.env;
import zep.frontend.sema.kind;
import zep.frontend.debug.sema_dumper;

export class AstDumper : public Visitor<void> {
  private:
    int depth;
    bool with_indent;
    bool trailing_newline;
    SemaDumper sema_dumper;

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

    void dump_type(const Type* type, int depth, bool with_indent = true,
                   bool trailing_newline = true) {
        sema_dumper.dump_type(type, depth, with_indent, trailing_newline);
    }

  public:
    explicit AstDumper(int depth = 0, bool with_indent = true, bool trailing_newline = true)
        : depth(depth), with_indent(with_indent), trailing_newline(trailing_newline) {}

    void dump_program(Program& program) {
        Logger::print_indent(0);
        Logger::print("Program(statements: [");

        if (program.statements.empty()) {
            Logger::print("])\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < program.statements.size(); ++i) {
                visit_child(*program.statements[i], 1, true, false);
                Logger::print((i + 1 < program.statements.size() ? ",\n" : "\n"));
            }
            Logger::print_indent(0);
            Logger::print("])\n");
        }
    }

    void visit(TypeExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("TypeExpression(type: ");
        if (node.type != nullptr) {
            dump_type(node.type, depth, false, false);
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
