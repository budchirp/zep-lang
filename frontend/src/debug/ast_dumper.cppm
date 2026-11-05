module;

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

export module zep.frontend.debug.ast_dumper;

import zep.common.logger;
import zep.common.state;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.sema.types;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.frontend.sema.scopes.environment;
import zep.frontend.kind;
import zep.frontend.debug.type_dumper;

export class AstDumper : public Visitor<void> {
  private:
    int depth;
    bool with_indent;
    bool trailing_newline;
    TypeDumper type_dumper;

    void visit_child(Node& node, int new_depth, bool new_indent, bool new_newline) {
        StateGuard depth_guard(depth, new_depth);
        StateGuard indent_guard(with_indent, new_indent);
        StateGuard newline_guard(trailing_newline, new_newline);
        visit_node(node);
    }

    void print_field(const char* name, Node* value, bool comma) {
        Logger::print_indent(depth + 1);
        Logger::print(name, ": ");

        if (value != nullptr) {
            visit_child(*value, depth + 1, false, false);
        } else {
            Logger::print("null");
        }

        Logger::print(comma ? ",\n" : "\n");
    }

    void end_node() {
        Logger::print_indent(depth);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    template <typename Child>
    void print_children(const char* label, const std::vector<Child*>& children, bool comma) {
        Logger::print_indent(depth + 1);
        Logger::print(label, ": [");
        if (!children.empty()) {
            Logger::print("\n");
            for (auto index = std::size_t{0}; index < children.size(); ++index) {
                visit_child(*children[index], depth + 2, true, false);
                Logger::print(index + 1 < children.size() ? ",\n" : "\n");
            }
            Logger::print_indent(depth + 1);
        }
        Logger::print(comma ? "],\n" : "]\n");
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
            type_dumper.dump(node.type, depth, false, false);
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

        end_node();
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
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        if (node.value != nullptr) {
            visit_child(*node.value, depth + 1, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print("\n");

        end_node();
    }

    void visit(Parameter& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("Parameter(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        print_field("type", node.type, false);
        end_node();
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

        end_node();
    }

    void visit(FunctionPrototype& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("FunctionPrototype(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("is_variadic: ", (node.is_variadic ? "true" : "false"), ",\n");

        print_children("generic_parameters", node.generic_parameters, true);

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

        end_node();
    }

    void visit(Field& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("Field(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(node.visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        print_field("type", node.type, false);
        end_node();
    }

    void visit(EnumVariant& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("EnumVariant(\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        print_children("fields", node.fields, false);

        end_node();
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

        end_node();
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

    void visit(CharLiteral& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("CharLiteral(value: ", static_cast<int>(node.value), ")");

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

    void visit(NullLiteral& node [[maybe_unused]]) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("NullLiteral()");

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

        end_node();
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

        end_node();
    }

    void visit(CoerceExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("CoerceExpression(value: ");
        visit_child(*node.value, depth, false, false);
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

        print_children("generic_arguments", node.generic_arguments, true);

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

        end_node();
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

        end_node();
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

        end_node();
    }

    void visit(QualifiedAccessExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("QualifiedAccessExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("parent: \"", node.parent, "\",\n");

        print_children("parent_generic_arguments", node.parent_generic_arguments, true);

        Logger::print_indent(depth + 1);
        Logger::print("member: \"", node.member, "\"\n");

        end_node();
    }

    void visit(NewExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("NewExpression(\n");

        if (node.storage != nullptr) {
            Logger::print_indent(depth + 1);
            Logger::print("storage: ");
            visit_child(*node.storage, depth + 1, false, false);
            Logger::print(",\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        visit_child(*node.value, depth + 1, false, false);
        Logger::print("\n");

        end_node();
    }

    void visit(DeleteExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("DeleteExpression(\n");
        Logger::print_indent(depth + 1);
        Logger::print("value: ");
        visit_child(*node.value, depth + 1, false, false);
        Logger::print("\n");

        end_node();
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

        end_node();
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

        print_children("generic_arguments", node.generic_arguments, true);

        print_children("fields", node.fields, false);

        end_node();
    }

    void visit(EnumVariantExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("EnumVariantExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("enum: ");
        visit_child(*node.enum_name, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("variant: \"", node.variant_name, "\",\n");

        print_children("fields", node.fields, true);

        end_node();
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

        end_node();
    }

    void visit(WhileStatement& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("WhileStatement(\n");
        print_field("condition", node.condition, true);
        print_field("body", node.body, false);
        end_node();
    }

    void visit(ForStatement& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ForStatement(\n");
        Logger::print_indent(depth + 1);
        Logger::print_indent(depth + 1);
        Logger::print("initializer: ");
        visit_child(*node.initializer, depth + 1, false, false);
        Logger::print(",\n");
        Logger::print_indent(depth + 1);
        Logger::print("condition: ");
        visit_child(*node.condition, depth + 1, false, false);
        Logger::print(",\n");
        Logger::print_indent(depth + 1);
        Logger::print("step: ");
        if (node.step != nullptr) {
            visit_child(*node.step, depth + 1, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print(",\n");
        Logger::print_indent(depth + 1);
        Logger::print("body: ");
        visit_child(*node.body, depth + 1, false, false);
        Logger::print("\n");
        end_node();
    }

    void visit(DeferStatement& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("DeferStatement(\n");
        print_field("body", node.body, false);
        end_node();
    }

    void visit(WhenPatternField& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("WhenPatternField(field: \"", node.field_name, "\", binding: \"",
                      node.binding_name, "\")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(WhenPattern& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("WhenPattern(\n");

        Logger::print_indent(depth + 1);
        Logger::print("kind: ",
                      node.pattern_kind == WhenPattern::PatternKind::Type::Expression
                          ? "Expression"
                          : "EnumVariant",
                      ",\n");

        if (node.pattern_kind == WhenPattern::PatternKind::Type::Expression) {
            Logger::print_indent(depth + 1);
            Logger::print("expression: ");
            visit_child(*node.expression, depth + 1, false, false);
            Logger::print("\n");
        } else {
            Logger::print_indent(depth + 1);
            Logger::print("enum: ");
            visit_child(*node.enum_name, depth + 1, false, false);
            Logger::print(",\n");

            Logger::print_indent(depth + 1);
            Logger::print("variant: \"", node.variant_name, "\",\n");

            print_children("fields", node.fields, false);
        }

        end_node();
    }

    void visit(WhenArm& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("WhenArm(\n");

        Logger::print_indent(depth + 1);
        Logger::print("is_else: ", (node.is_else ? "true" : "false"), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("patterns: [");
        if (node.patterns.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.patterns.size(); ++i) {
                visit_child(*node.patterns[i], depth + 2, true, false);
                Logger::print((i + 1 < node.patterns.size() ? ",\n" : "\n"));
            }
            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("guard: ");
        if (node.guard != nullptr) {
            visit_child(*node.guard, depth + 1, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("body: ");
        visit_child(*node.body, depth + 1, false, false);
        Logger::print("\n");

        end_node();
    }

    void visit(WhenExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("WhenExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("subject: ");
        if (node.subject != nullptr) {
            visit_child(*node.subject, depth + 1, false, false);
        } else {
            Logger::print("null");
        }
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("arms: [");
        if (node.arms.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.arms.size(); ++i) {
                visit_child(*node.arms[i], depth + 2, true, false);
                Logger::print((i + 1 < node.arms.size() ? ",\n" : "\n"));
            }
            Logger::print_indent(depth + 1);
            Logger::print("]\n");
        }

        end_node();
    }

    void visit(AssemblyExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("AssemblyExpression(template: \"", node.text, "\", arguments: [\n");

        for (auto index = std::size_t{0}; index < node.arguments.size(); ++index) {
            visit_child(*node.arguments[index], depth + 1, true, false);
            Logger::print(index + 1 < node.arguments.size() ? ",\n" : "\n");
        }

        Logger::print_indent(depth);
        Logger::print("])");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(BuiltinCall& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("BuiltinCall(name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("type_argument: ");
        if (node.type_argument != nullptr) {
            visit_child(*node.type_argument, depth + 1, false, false);
        } else {
            Logger::print("null");
        }

        if (!node.arguments.empty()) {
            Logger::print(",\n");
            Logger::print_indent(depth + 1);
            Logger::print("arguments: [\n");
            for (std::size_t i = 0; i < node.arguments.size(); ++i) {
                visit_child(*node.arguments[i], depth + 2, true, false);
                Logger::print((i + 1 < node.arguments.size() ? ",\n" : "\n"));
            }
            Logger::print_indent(depth + 1);
            Logger::print("]");
        }

        Logger::print(")\n");

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

    void visit(InterfaceDeclaration& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("InterfaceDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(node.visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("methods: [");
        if (node.methods.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.methods.size(); ++i) {
                visit_child(*node.methods[i], depth + 2, true, false);
                Logger::print((i + 1 < node.methods.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("]\n");
        }

        end_node();
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

        print_children("generic_parameters", node.generic_parameters, true);

        print_children("fields", node.fields, false);

        end_node();
    }

    void visit(EnumDeclaration& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("EnumDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(node.visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("name: \"", node.name, "\",\n");

        print_children("generic_parameters", node.generic_parameters, true);
        print_children("variants", node.variants, false);

        end_node();
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

        end_node();
    }

    void visit(FunctionDeclaration& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("FunctionDeclaration(\n");

        Logger::print_indent(depth + 1);
        Logger::print("visibility: ", Visibility::to_string(node.visibility), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("kind: ", FunctionSymbol::role_name(node.role()), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("is_static: ", (node.is_static ? "true" : "false"), ",\n");

        Logger::print_indent(depth + 1);
        Logger::print("parent: \"", node.parent, "\",\n");

        Logger::print_indent(depth + 1);
        Logger::print("prototype: ");
        visit_child(*node.prototype, depth + 1, false, false);
        Logger::print(",\n");

        Logger::print_indent(depth + 1);
        Logger::print("body: ");
        visit_child(*node.body, depth + 1, false, false);
        Logger::print("\n");

        end_node();
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

        end_node();
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

        end_node();
    }

    void visit(ImportStatement& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ImportStatement(path: [");
        if (node.path.empty()) {
            Logger::print("], alias: {})", node.alias);
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.path.size(); ++i) {
                visit_child(*node.path[i], depth + 1, true, false);
                Logger::print((i + 1 < node.path.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth);
            Logger::print("], alias: {})", node.alias);
        }

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(TypeAliasDeclaration& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("TypeAliasDeclaration(visibility: {}, name: \"", "{}\")\n",
                      node.visibility == Visibility::Type::Public ? "public" : "private",
                      node.name);

        if (!node.generic_parameters.empty()) {
            print_children("generic_parameters", node.generic_parameters, false);
        }

        Logger::print_indent(depth + 1);
        Logger::print("target: ");
        visit_child(*node.target, depth + 1, false, false);
        Logger::print("\n");

        end_node();
    }

    void visit(ClosureExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ClosureExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("parameters: [");
        if (node.parameters.empty()) {
            Logger::print("],\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.parameters.size(); ++i) {
                Logger::print_indent(depth + 2);
                Logger::print("Parameter(name: \"", node.parameters[i]->name, "\"");
                if (node.parameters[i]->type != nullptr) {
                    Logger::print(", type: ");
                    visit_child(*node.parameters[i]->type, depth + 2, false, false);
                }
                Logger::print(")");
                Logger::print((i + 1 < node.parameters.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("],\n");
        }

        Logger::print_indent(depth + 1);
        Logger::print("body: ");
        visit_child(*node.body, depth + 1, false, false);
        Logger::print("\n");

        end_node();
    }

    void visit(BlockExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("BlockExpression(body: ");
        visit_child(*node.body, depth + 1, false, false);
        Logger::print(")");

        if (trailing_newline) {
            Logger::print("\n");
        }
    }

    void visit(ArrayLiteralExpression& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("ArrayLiteralExpression(\n");

        Logger::print_indent(depth + 1);
        Logger::print("elements: [");
        if (node.elements.empty()) {
            Logger::print("]\n");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.elements.size(); ++i) {
                visit_child(*node.elements[i], depth + 2, true, false);
                Logger::print((i + 1 < node.elements.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth + 1);
            Logger::print("]\n");
        }

        end_node();
    }

    void visit(Attribute& node) override {
        if (with_indent) {
            Logger::print_indent(depth);
        }

        Logger::print("Attribute(name: \"", node.name, "\", arguments: [");
        if (node.arguments.empty()) {
            Logger::print("])");
        } else {
            Logger::print("\n");
            for (std::size_t i = 0; i < node.arguments.size(); ++i) {
                visit_child(*node.arguments[i], depth + 1, true, false);
                Logger::print((i + 1 < node.arguments.size() ? ",\n" : "\n"));
            }

            Logger::print_indent(depth);
            Logger::print("])");
        }

        if (trailing_newline) {
            Logger::print("\n");
        }
    }
};
