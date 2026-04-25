module;

#include <cstdint>
#include <string>
#include <vector>

export module zep.hir.debug.dumper;

import zep.common.logger;
import zep.frontend.sema.type;
import zep.frontend.sema.kind;
import zep.hir.ir;

export class HIRDumper : public HIRVisitor<void> {
  private:
    std::uint32_t indent_level = 0;

    void print_indent() {
        for (std::uint32_t i = 0; i < indent_level; ++i) {
            Logger::print("  ");
        }
    }

    void print_type(const Type* type) {
        if (type != nullptr) {
            Logger::print(" (type: " + type->to_string() + ")");
        }
    }

  public:
    HIRDumper() = default;

    void dump_program(HIRProgram& program) {
        Logger::print("HIRProgram([\n");
        indent_level++;

        for (std::size_t i = 0; i < program.functions.size(); ++i) {
            program.functions[i]->accept(*this);
            if (i < program.functions.size() - 1) {
                Logger::print(",\n");
            }
        }

        indent_level--;
        Logger::print("\n])\n");
    }

    void visit(HIRBlockStatement& node) override {
        print_indent();
        Logger::print("BlockStatement([\n");
        indent_level++;

        for (std::size_t i = 0; i < node.statements.size(); ++i) {
            node.statements[i]->accept(*this);
            if (i < node.statements.size() - 1) {
                Logger::print(",\n");
            }
        }

        indent_level--;
        Logger::print("\n");
        print_indent();
        Logger::print("])");
    }

    void visit(HIRNumberLiteral& node) override {
        print_indent();
        Logger::print("NumberLiteral(value: \"" + node.value + "\"");
        print_type(node.type);
        Logger::print(")");
    }

    void visit(HIRFloatLiteral& node) override {
        print_indent();
        Logger::print("FloatLiteral(value: \"" + node.value + "\"");
        print_type(node.type);
        Logger::print(")");
    }

    void visit(HIRStringLiteral& node) override {
        print_indent();
        Logger::print("StringLiteral(value: \"" + node.value + "\"");
        print_type(node.type);
        Logger::print(")");
    }

    void visit(HIRBooleanLiteral& node) override {
        print_indent();
        Logger::print("BooleanLiteral(value: " + std::string(node.value ? "true" : "false"));
        print_type(node.type);
        Logger::print(")");
    }

    void visit(HIRIdentifierExpression& node) override {
        print_indent();
        Logger::print("IdentifierExpression(name: \"" + node.name + "\"");
        print_type(node.type);
        Logger::print(")");
    }

    void visit(HIRBinaryExpression& node) override {
        print_indent();
        Logger::print("BinaryExpression(\n");
        indent_level++;

        print_indent();
        Logger::print("left:\n");
        indent_level++;
        node.left->accept(*this);
        indent_level--;
        Logger::print(",\n");

        print_indent();
        Logger::print("op: " + HIRBinaryExpression::Operator::to_string(node.op) + ",\n");

        print_indent();
        Logger::print("right:\n");
        indent_level++;
        node.right->accept(*this);
        indent_level--;
        Logger::print("\n");

        indent_level--;
        print_indent();
        Logger::print(")");
        print_type(node.type);
    }

    void visit(HIRUnaryExpression& node) override {
        print_indent();
        Logger::print("UnaryExpression(\n");
        indent_level++;

        print_indent();
        Logger::print("op: " + HIRUnaryExpression::Operator::to_string(node.op) + ",\n");

        print_indent();
        Logger::print("operand:\n");
        indent_level++;
        node.operand->accept(*this);
        indent_level--;
        Logger::print("\n");

        indent_level--;
        print_indent();
        Logger::print(")");
        print_type(node.type);
    }

    void visit(HIRCallExpression& node) override {
        print_indent();
        Logger::print("CallExpression(\n");
        indent_level++;

        print_indent();
        Logger::print("callee:\n");
        indent_level++;
        node.callee->accept(*this);
        indent_level--;
        Logger::print(",\n");

        print_indent();
        Logger::print("arguments: [\n");
        indent_level++;

        for (std::size_t i = 0; i < node.arguments.size(); ++i) {
            node.arguments[i]->accept(*this);
            if (i < node.arguments.size() - 1) {
                Logger::print(",\n");
            }
        }

        indent_level--;
        Logger::print("\n");
        print_indent();
        Logger::print("]\n");

        indent_level--;
        print_indent();
        Logger::print(")");
        print_type(node.type);
    }

    void visit(HIRIndexExpression& node) override {
        print_indent();
        Logger::print("IndexExpression(\n");
        indent_level++;

        print_indent();
        Logger::print("object:\n");
        indent_level++;
        node.object->accept(*this);
        indent_level--;
        Logger::print(",\n");

        print_indent();
        Logger::print("index:\n");
        indent_level++;
        node.index->accept(*this);
        indent_level--;
        Logger::print("\n");

        indent_level--;
        print_indent();
        Logger::print(")");
        print_type(node.type);
    }

    void visit(HIRMemberExpression& node) override {
        print_indent();
        Logger::print("MemberExpression(\n");
        indent_level++;

        print_indent();
        Logger::print("object:\n");
        indent_level++;
        node.object->accept(*this);
        indent_level--;
        Logger::print(",\n");

        print_indent();
        Logger::print("member: \"" + node.member + "\"\n");

        indent_level--;
        print_indent();
        Logger::print(")");
        print_type(node.type);
    }

    void visit(HIRAssignExpression& node) override {
        print_indent();
        Logger::print("AssignExpression(\n");
        indent_level++;

        print_indent();
        Logger::print("target:\n");
        indent_level++;
        node.target->accept(*this);
        indent_level--;
        Logger::print(",\n");

        print_indent();
        Logger::print("value:\n");
        indent_level++;
        node.value->accept(*this);
        indent_level--;
        Logger::print("\n");

        indent_level--;
        print_indent();
        Logger::print(")");
        print_type(node.type);
    }

    void visit(HIRStructLiteralExpression& node) override {
        print_indent();
        Logger::print("StructLiteralExpression(name: \"" + node.name + "\", fields: [\n");
        indent_level++;

        for (std::size_t i = 0; i < node.fields.size(); ++i) {
            print_indent();
            Logger::print("HIRStructLiteralField(name: \"" + node.fields[i].name + "\", value:\n");
            indent_level++;
            node.fields[i].value->accept(*this);
            indent_level--;
            Logger::print(")");
            if (i < node.fields.size() - 1) {
                Logger::print(",\n");
            }
        }

        indent_level--;
        Logger::print("\n");
        print_indent();
        Logger::print("])");
        print_type(node.type);
    }

    void visit(HIRIfExpression& node) override {
        print_indent();
        Logger::print("IfExpression(\n");
        indent_level++;

        print_indent();
        Logger::print("condition:\n");
        indent_level++;
        node.condition->accept(*this);
        indent_level--;
        Logger::print(",\n");

        print_indent();
        Logger::print("then:\n");
        indent_level++;
        node.then_branch->accept(*this);
        indent_level--;

        if (node.else_branch != nullptr) {
            Logger::print(",\n");
            print_indent();
            Logger::print("else:\n");
            indent_level++;
            node.else_branch->accept(*this);
            indent_level--;
        }

        indent_level--;
        Logger::print("\n");
        print_indent();
        Logger::print(")");
        print_type(node.type);
    }

    void visit(HIRTypeExpression& node) override {
        print_indent();
        Logger::print("TypeExpression(value: " + node.type_value->to_string() + ")");
    }

    void visit(HIRExpressionStatement& node) override {
        print_indent();
        Logger::print("ExpressionStatement(expression:\n");
        indent_level++;
        node.expression->accept(*this);
        indent_level--;
        Logger::print("\n");
        print_indent();
        Logger::print(")");
    }

    void visit(HIRReturnStatement& node) override {
        print_indent();
        Logger::print("ReturnStatement(value:\n");
        if (node.value != nullptr) {
            indent_level++;
            node.value->accept(*this);
            indent_level--;
        } else {
            Logger::print("  null");
        }
        Logger::print("\n");
        print_indent();
        Logger::print(")");
    }

    void visit(HIRVarDeclaration& node) override {
        print_indent();
        Logger::print("VarDeclaration(name: \"" + node.name + "\", visibility: " + Visibility::to_string(node.visibility) + ", storage: " + StorageKind::to_string(node.storage_kind));
        print_type(node.type);
        if (node.initializer != nullptr) {
            Logger::print(", initializer:\n");
            indent_level++;
            node.initializer->accept(*this);
            indent_level--;
        }
        Logger::print(")");
    }

    void visit(HIRFunctionDeclaration& node) override {
        print_indent();
        Logger::print("FunctionDeclaration(name: \"" + node.name + "\", visibility: " + Visibility::to_string(node.visibility));
        Logger::print(", parameters: [\n");
        indent_level++;

        for (std::size_t i = 0; i < node.parameters.size(); ++i) {
            print_indent();
            Logger::print("Parameter(name: \"" + node.parameters[i].name + "\", type: " + node.parameters[i].type->to_string() + ")");
            if (i < node.parameters.size() - 1) {
                Logger::print(",\n");
            }
        }

        indent_level--;
        Logger::print("\n");
        print_indent();
        Logger::print("], return_type: " + node.return_type->to_string() + ",\n");

        print_indent();
        Logger::print("body:\n");
        indent_level++;
        node.body->accept(*this);
        indent_level--;
        Logger::print("\n");
        print_indent();
        Logger::print(")");
    }
};
