module;

#include <cstddef>
#include <cstdint>
#include <string>

export module zep.lsp.analysis.finder;

import zep.common.source.position;
import zep.common.source.span;
import zep.frontend.node;
import zep.frontend.sema.kind;

export class NodeFinder : public Visitor<void> {
  private:
    ::Position target;

    bool contains(Span span) const {
        if (span.start.line == 0) {
            return false;
        }

        if (target.line < span.start.line || target.line > span.end.line) {
            return false;
        }

        if (target.line == span.start.line && target.column < span.start.column) {
            return false;
        }

        if (target.line == span.end.line && target.column > span.end.column) {
            return false;
        }

        return true;
    }

    void check_node(Node& node) {
        if (!contains(node.span)) {
            return;
        }

        if (innermost == nullptr) {
            innermost = &node;
            return;
        }

        if (node.span.start.line >= innermost->span.start.line &&
            node.span.end.line <= innermost->span.end.line) {
            innermost = &node;
        }
    }

  public:
    Node* innermost = nullptr;

    explicit NodeFinder(::Position target) : target(target) {}

    void visit_child(Node* child) {
        if (child != nullptr) {
            visit_node(*child);
        }
    }

    void visit(TypeExpression& node) override { check_node(node); }

    void visit(Attribute& node) override {
        check_node(node);
        for (auto* argument : node.arguments) {
            visit_child(argument);
        }
    }

    void visit(GenericParameter& node) override {
        check_node(node);
        visit_child(node.constraint);
    }

    void visit(GenericArgument& node) override {
        check_node(node);
        visit_child(node.type);
        visit_child(node.value);
    }

    void visit(Parameter& node) override {
        check_node(node);
        visit_child(node.type);
    }

    void visit(Argument& node) override {
        check_node(node);
        visit_child(node.value);
    }

    void visit(FunctionPrototype& node) override {
        for (auto* parameter : node.parameters) {
            visit_child(parameter);
        }
        visit_child(node.return_type);
    }

    void visit(Field& node) override {
        check_node(node);
        visit_child(node.type);
        visit_child(node.default_value);
    }

    void visit(EnumVariant& node) override {
        check_node(node);
        for (auto* field : node.fields) {
            visit_child(field);
        }
        visit_child(node.value_expression);
    }

    void visit(StructLiteralField& node) override {
        check_node(node);
        visit_child(node.value);
    }

    void visit(WhenPatternField& node) override { check_node(node); }

    void visit(WhenPattern& node) override { check_node(node); }

    void visit(WhenArm& node) override {
        check_node(node);
        visit_child(node.body);
    }

    void visit(NumberLiteral& node) override { check_node(node); }

    void visit(FloatLiteral& node) override { check_node(node); }

    void visit(StringLiteral& node) override { check_node(node); }

    void visit(CharLiteral& node) override { check_node(node); }

    void visit(BooleanLiteral& node) override { check_node(node); }

    void visit(NullLiteral& node) override { check_node(node); }

    void visit(IdentifierExpression& node) override { check_node(node); }

    void visit(BinaryExpression& node) override {
        check_node(node);
        visit_child(node.left);
        visit_child(node.right);
    }

    void visit(UnaryExpression& node) override {
        check_node(node);
        visit_child(node.operand);
    }

    void visit(CoerceExpression& node) override {
        check_node(node);
        visit_child(node.value);
    }

    void visit(CallExpression& node) override {
        check_node(node);
        visit_child(node.callee);
        for (auto* argument : node.arguments) {
            visit_child(argument);
        }
    }

    void visit(IndexExpression& node) override {
        check_node(node);
        visit_child(node.value);
        visit_child(node.index);
    }

    void visit(MemberExpression& node) override {
        check_node(node);
        visit_child(node.value);
    }

    void visit(QualifiedAccessExpression& node) override { check_node(node); }

    void visit(AssignExpression& node) override {
        check_node(node);
        visit_child(node.target);
        visit_child(node.value);
    }

    void visit(StructLiteralExpression& node) override {
        check_node(node);
        for (auto* field : node.fields) {
            visit_child(field);
        }
    }

    void visit(EnumVariantExpression& node) override { check_node(node); }

    void visit(IfExpression& node) override {
        check_node(node);
        visit_child(node.condition);
        visit_child(node.then_branch);
        visit_child(node.else_branch);
    }

    void visit(WhenExpression& node) override {
        check_node(node);
        visit_child(node.subject);
        for (auto* arm : node.arms) {
            visit_child(arm);
        }
    }

    void visit(ClosureExpression& node) override {
        check_node(node);
        for (auto* parameter : node.parameters) {
            visit_child(parameter);
        }
        visit_child(node.body);
    }

    void visit(BlockExpression& node) override {
        check_node(node);
        visit_child(node.body);
    }

    void visit(BuiltinCall& node) override {
        check_node(node);
        for (auto* argument : node.arguments) {
            visit_child(argument);
        }
    }

    void visit(ArrayLiteralExpression& node) override {
        check_node(node);
        for (auto* element : node.elements) {
            visit_child(element);
        }
    }

    void visit(BlockStatement& node) override {
        check_node(node);
        for (auto* statement : node.statements) {
            visit_child(statement);
        }
    }

    void visit(ExpressionStatement& node) override {
        check_node(node);
        visit_child(node.expression);
    }

    void visit(ReturnStatement& node) override {
        check_node(node);
        visit_child(node.value);
    }

    void visit(WhileStatement& node) override {
        check_node(node);
        visit_child(node.condition);
        visit_child(node.body);
    }

    void visit(ForStatement& node) override {
        check_node(node);
        visit_child(node.initializer);
        visit_child(node.condition);
        visit_child(node.step);
        visit_child(node.body);
    }

    void visit(DeferStatement& node) override {
        check_node(node);
        visit_child(node.body);
    }

    void visit(InterfaceDeclaration& node) override {
        check_node(node);
        for (auto* method : node.methods) {
            visit_child(method);
        }
    }

    void visit(StructDeclaration& node) override {
        auto name_start = node.span.start.column + 7U;
        auto name_end = name_start + static_cast<std::uint32_t>(node.name.length());
        if (target.line == node.span.start.line && target.column >= node.span.start.column &&
            target.column <= name_end) {
            innermost = &node;
        }
        check_node(node);
        for (auto* field : node.fields) {
            visit_child(field);
        }
        for (auto* method : node.methods) {
            visit_child(method);
        }
    }

    void visit(EnumDeclaration& node) override {
        auto name_start = node.span.start.column + 5U;
        auto name_end = name_start + static_cast<std::uint32_t>(node.name.length());
        if (target.line == node.span.start.line && target.column >= node.span.start.column &&
            target.column <= name_end) {
            innermost = &node;
        }
        check_node(node);
        for (auto* variant : node.variants) {
            visit_child(variant);
        }
    }

    void visit(VarDeclaration& node) override {
        auto prefix_len = (node.storage_kind == StorageKind::Type::VarMut) ? 8U : 4U;
        auto name_start = node.span.start.column + prefix_len;
        auto name_end = name_start + static_cast<std::uint32_t>(node.name.length());
        if (target.line == node.span.start.line && target.column >= node.span.start.column &&
            target.column <= name_end) {
            innermost = &node;
        }
        check_node(node);
        visit_child(node.annotation);
        visit_child(node.initializer);
    }

    void visit(FunctionDeclaration& node) override {
        if (node.prototype != nullptr) {
            if (target.line == node.prototype->span.start.line &&
                target.column >= node.prototype->span.start.column &&
                target.column <= node.prototype->span.end.column) {
                innermost = &node;
            }
        }
        check_node(node);
        visit_child(node.prototype);
        visit_child(node.body);
    }

    void visit(ExternFunctionDeclaration& node) override {
        check_node(node);
        visit_child(node.prototype);
    }

    void visit(ExternVarDeclaration& node) override {
        check_node(node);
        visit_child(node.annotation);
    }

    void visit(ImportStatement& node) override { check_node(node); }

    void visit(TypeAliasDeclaration& node) override {
        check_node(node);
        visit_child(node.target);
    }
};
