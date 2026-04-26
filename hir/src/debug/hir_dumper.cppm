module;

#include <cstdint>
#include <string>
#include <vector>

export module zep.hir.debug.dumper;

import zep.common.logger;
import zep.frontend.sema.type;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
import zep.frontend.sema.symbol;
import zep.frontend.debug.sema_dumper;
import zep.hir.node;
import zep.hir.node.program;

export class HIRDumper : public HIRVisitor<void> {
  private:
    int depth = 0;
    bool with_indent = true;
    SemaDumper sema_dumper;

    void visit_child(HIRNode& node, int new_depth, bool new_with_indent = true) {
        int saved_depth = depth;
        bool saved_with_indent = with_indent;

        depth = new_depth;
        with_indent = new_with_indent;
        node.accept(*this);

        depth = saved_depth;
        with_indent = saved_with_indent;
    }

    void dump_type(const Type* type, int depth, bool with_indent = true,
                   bool trailing_newline = false) {
        sema_dumper.dump_type(type, depth, with_indent, trailing_newline);
    }

    void dump_symbol(const Symbol& symbol, int depth) {
        Logger::print_indent(depth);
        Logger::print("Symbol(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", symbol.name, "\",\n");

        Logger::print_indent(depth + 1);
        auto kind_str = std::string();
        switch (symbol.kind) {
        case Symbol::Kind::Type::Var:
            kind_str = "Var";
            break;
        case Symbol::Kind::Type::Function:
            kind_str = "Function";
            break;
        case Symbol::Kind::Type::Type:
            kind_str = "Type";
            break;
        }
        Logger::print("kind: ", kind_str, ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(symbol.visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(symbol.type, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void dump_scope(const Scope& scope) {
        Logger::print_indent(1);
        Logger::print("symbols: [\n");

        auto total_symbols = scope.variables.size() + scope.types.size();
        for (const auto& [name, overloads] : scope.functions) {
            total_symbols += overloads.size();
        }

        auto current = std::size_t(0);

        for (const auto& [name, symbol] : scope.variables) {
            dump_symbol(*symbol, 2);
            if (++current < total_symbols) {
                Logger::print(",");
            }
            Logger::print("\n");
        }

        for (const auto& [name, symbol] : scope.types) {
            dump_symbol(*symbol, 2);
            if (++current < total_symbols) {
                Logger::print(",");
            }
            Logger::print("\n");
        }

        for (const auto& [name, overloads] : scope.functions) {
            for (const auto* symbol : overloads) {
                dump_symbol(*symbol, 2);
                if (++current < total_symbols) {
                    Logger::print(",");
                }
                Logger::print("\n");
            }
        }

        Logger::print_indent(1);
        Logger::print("]\n");
    }

  public:
    HIRDumper() = default;

    void dump_program(HIRProgram& program) {
        Logger::print_indent(0);
        Logger::print("HIRProgram(\n");

        if (program.global_scope != nullptr) {
            dump_scope(*program.global_scope);
        }

        Logger::print_indent(1);
        Logger::print("statements: [\n");

        for (std::size_t i = 0; i < program.statements.size(); ++i) {
            visit_child(*program.statements[i], 2, true);
            Logger::print((i + 1 < program.statements.size() ? ",\n" : "\n"));
        }

        Logger::print_indent(1);
        Logger::print("],\n");

        Logger::print_indent(1);
        Logger::print("functions: [\n");

        for (std::size_t i = 0; i < program.functions.size(); ++i) {
            visit_child(*program.functions[i], 2, true);
            Logger::print((i + 1 < program.functions.size() ? ",\n" : "\n"));
        }

        Logger::print_indent(1);
        Logger::print("]\n");

        Logger::print_indent(0);
        Logger::print(")\n");
    }

    void visit(HIRBlockStatement& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("BlockStatement(statements: [");
        if (node.statements.empty()) {
            Logger::print("])");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.statements.size(); ++i) {
                visit_child(*node.statements[i], depth + 1, true);
                Logger::print((i + 1 < node.statements.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth);
            Logger::print("])");
        }
    }

    void visit(HIRNumberLiteral& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("NumberLiteral(\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: \"", node.value, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRFloatLiteral& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("FloatLiteral(\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: \"", node.value, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRStringLiteral& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("StringLiteral(\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: \"", node.value, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRBooleanLiteral& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("BooleanLiteral(\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: ", (node.value ? "true" : "false"), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRIdentifierExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("IdentifierExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRBinaryExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("BinaryExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("op: ", HIRBinaryExpression::Operator::to_string(node.op), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("left: ");
        visit_child(*node.left, depth + 1, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("right: ");
        visit_child(*node.right, depth + 1, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRUnaryExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("UnaryExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("op: ", HIRUnaryExpression::Operator::to_string(node.op), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("operand: ");
        visit_child(*node.operand, depth + 1, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRCallExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("CallExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("callee: ");
        visit_child(*node.callee, depth + 1, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("arguments: [");
        if (node.arguments.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.arguments.size(); ++i) {
                visit_child(*node.arguments[i], depth + 2, true);
                Logger::print((i + 1 < node.arguments.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("]\n");
        }

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRIndexExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("IndexExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("object: ");
        visit_child(*node.object, depth + 1, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("index: ");
        visit_child(*node.index, depth + 1, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRMemberExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("MemberExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("member: \"", node.member, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("object: ");
        visit_child(*node.object, depth + 1, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRAssignExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("AssignExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("target: ");
        visit_child(*node.target, depth + 1, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        visit_child(*node.value, depth + 1, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRStructLiteralExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("StructLiteralExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("fields: [");
        if (node.fields.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.fields.size(); ++i) {
                Logger::print_indent(depth + 2);
                Logger::print("Field(\n");

                Logger::print_indent(depth + 3);
                Logger::print("name: \"", node.fields[i].name, "\",\n");

                Logger::print_indent(depth + 3);
                Logger::print("value: ");
                visit_child(*node.fields[i].value, depth + 3, false);
                Logger::print("\n");

                Logger::print_indent(depth + 2);
                Logger::print(")");

                Logger::print((i + 1 < node.fields.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("]\n");
        }

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRIfExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("IfExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("condition: ");
        visit_child(*node.condition, depth + 1, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("then: ");
        visit_child(*node.then_branch, depth + 1, false);

        if (node.else_branch != nullptr) {
            Logger::print(",\n");
            Logger::print_indent(depth + 1);
            Logger::print("else: ");
            visit_child(*node.else_branch, depth + 1, false);
        }
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRTypeExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("TypeExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type_value, depth + 1, false, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRExpressionStatement& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ExpressionStatement(\n");

        Logger::print_indent(depth + 1);
        Logger::print("expression: ");
        visit_child(*node.expression, depth + 1, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRReturnStatement& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ReturnStatement(\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        if (node.value != nullptr) {
            visit_child(*node.value, depth + 1, false);
        } else {
            Logger::print("null");
        }
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRVarDeclaration& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("VarDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("storage: ", StorageKind::to_string(node.storage_kind), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type: ");
        dump_type(node.type, depth + 1, false, false);
        if (node.initializer != nullptr) {
            Logger::print(",\n");

            Logger::print_indent(depth + 1);
            Logger::print("initializer: ");
            visit_child(*node.initializer, depth + 1, false);
        }
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }

    void visit(HIRFunctionDeclaration& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("FunctionDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(node.visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("parameters: [");
        if (node.parameters.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.parameters.size(); ++i) {
                Logger::print_indent(depth + 2);
                Logger::print("Parameter(\n");

                Logger::print_indent(depth + 3);
                Logger::print("name: \"", node.parameters[i].name, "\",\n");

                Logger::print_indent(depth + 3);
                Logger::print("type: ");
                dump_type(node.parameters[i].type, depth + 3, false, false);
                Logger::print("\n");

                Logger::print_indent(depth + 2);
                Logger::print(")");

                Logger::print((i + 1 < node.parameters.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("return_type: ");
        dump_type(node.return_type, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("body: ");
        visit_child(*node.body, depth + 1, false);
        Logger::print("\n");

        Logger::print_indent(depth);
        Logger::print(")");
    }
};
