module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module zep.lsp.analysis.index;

import zep.common.source.position;
import zep.common.source.span;
import zep.frontend.lexer;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.sema.kind;
import zep.frontend.sema.scope;
import zep.frontend.token;
import zep.frontend.token.keywords;
import zep.lsp.analysis.types;

class RawToken {
  public:
    std::uint32_t line;
    std::uint32_t character;
    std::uint32_t length;
    SemanticKind::Type kind;
    std::uint32_t modifiers;

    RawToken(std::uint32_t line, std::uint32_t character, std::uint32_t length,
             SemanticKind::Type kind, std::uint32_t modifiers = 0)
        : line(line), character(character), length(length), kind(kind), modifiers(modifiers) {}
};

class AstTokenCollector : public Visitor<void> {
  public:
    std::vector<RawToken> tokens;
    std::vector<Node*> nodes;

    void add_token(std::uint32_t line, std::uint32_t character, std::uint32_t length,
                   SemanticKind::Type kind, std::uint32_t modifiers = 0) {
        tokens.emplace_back(line, character, length, kind, modifiers);
    }

    void visit_child(Node* child) {
        if (child != nullptr) {
            nodes.push_back(child);
            visit_node(*child);
        }
    }

    void visit(TypeExpression& node) override {
        if (node.span.start.line > 0 && node.type != nullptr) {
            auto line = static_cast<std::uint32_t>(node.span.start.line - 1);
            auto character = static_cast<std::uint32_t>(node.span.start.column - 1);
            auto length =
                static_cast<std::uint32_t>(node.span.end.column - node.span.start.column + 1);
            add_token(line, character, length, SemanticKind::Type::Type);
        }
    }

    void visit(Attribute& node) override {
        for (auto* argument : node.arguments) {
            visit_child(argument);
        }
    }

    void visit(GenericParameter& node) override { visit_child(node.constraint); }

    void visit(GenericArgument& node) override {
        visit_child(node.type);
        visit_child(node.value);
    }

    void visit(Parameter& node) override {
        if (node.span.start.line > 0 && !node.name.empty()) {
            auto line = static_cast<std::uint32_t>(node.span.start.line - 1);
            auto character = static_cast<std::uint32_t>(node.span.start.column - 1);
            auto length = static_cast<std::uint32_t>(node.name.length());
            add_token(line, character, length, SemanticKind::Type::Parameter,
                      SemanticModifier::Declaration);
        }
        visit_child(node.type);
    }

    void visit(Argument& node) override { visit_child(node.value); }

    void visit(FunctionDeclaration& node) override {
        if (node.prototype != nullptr && node.prototype->span.start.line > 0) {
            auto line = static_cast<std::uint32_t>(node.prototype->span.start.line - 1);
            auto character = static_cast<std::uint32_t>(node.prototype->span.start.column - 1);
            auto length = static_cast<std::uint32_t>(node.prototype->name.length());
            add_token(line, character, length, SemanticKind::Type::Function,
                      SemanticModifier::Declaration | SemanticModifier::Definition);
        }

        visit_child(node.prototype);
        visit_child(node.body);
    }

    void visit(FunctionPrototype& node) override {
        for (auto* parameter : node.parameters) {
            visit_child(parameter);
        }
        visit_child(node.return_type);
    }

    void visit(Field& node) override {
        if (node.span.start.line > 0 && !node.name.empty()) {
            auto line = static_cast<std::uint32_t>(node.span.start.line - 1);
            auto character = static_cast<std::uint32_t>(node.span.start.column - 1);
            auto length = static_cast<std::uint32_t>(node.name.length());
            add_token(line, character, length, SemanticKind::Type::Property,
                      SemanticModifier::Declaration);
        }
        visit_child(node.type);
        visit_child(node.default_value);
    }

    void visit(EnumVariant& node) override {
        if (node.span.start.line > 0 && !node.name.empty()) {
            auto line = static_cast<std::uint32_t>(node.span.start.line - 1);
            auto character = static_cast<std::uint32_t>(node.span.start.column - 1);
            auto length = static_cast<std::uint32_t>(node.name.length());
            add_token(line, character, length, SemanticKind::Type::EnumMember,
                      SemanticModifier::Declaration);
        }
        for (auto* field : node.fields) {
            visit_child(field);
        }
        visit_child(node.value_expression);
    }

    void visit(StructLiteralField& node) override { visit_child(node.value); }

    void visit(WhenPatternField&) override {}

    void visit(WhenPattern&) override {}

    void visit(WhenArm& node) override { visit_child(node.body); }

    void visit(NumberLiteral&) override {}

    void visit(FloatLiteral&) override {}

    void visit(StringLiteral&) override {}

    void visit(CharLiteral&) override {}

    void visit(BooleanLiteral&) override {}

    void visit(NullLiteral&) override {}

    void visit(IdentifierExpression& node) override {
        if (node.span.start.line == 0 || node.name.empty()) {
            return;
        }

        auto line = static_cast<std::uint32_t>(node.span.start.line - 1);
        auto character = static_cast<std::uint32_t>(node.span.start.column - 1);
        auto length = static_cast<std::uint32_t>(node.name.length());

        if (node.function_symbol != nullptr) {
            add_token(line, character, length, SemanticKind::Type::Function);
        } else if (node.var_symbol != nullptr) {
            add_token(line, character, length, SemanticKind::Type::Variable);
        }
    }

    void visit(BinaryExpression& node) override {
        visit_child(node.left);
        visit_child(node.right);
    }

    void visit(UnaryExpression& node) override { visit_child(node.operand); }

    void visit(CoerceExpression& node) override { visit_child(node.value); }

    void visit(CallExpression& node) override {
        if (auto* id = node.callee->as<IdentifierExpression>(); id != nullptr) {
            nodes.push_back(id);
            if (id->span.start.line > 0 && !id->name.empty()) {
                auto line = static_cast<std::uint32_t>(id->span.start.line - 1);
                auto character = static_cast<std::uint32_t>(id->span.start.column - 1);
                auto length = static_cast<std::uint32_t>(id->name.length());
                add_token(line, character, length, SemanticKind::Type::Function);
            }
        } else {
            visit_child(node.callee);
        }

        for (auto* argument : node.arguments) {
            visit_child(argument);
        }
    }

    void visit(IndexExpression& node) override {
        visit_child(node.value);
        visit_child(node.index);
    }

    void visit(MemberExpression& node) override {
        visit_child(node.value);
        if (node.span.start.line > 0 && !node.member.empty()) {
            auto line = static_cast<std::uint32_t>(node.span.end.line - 1);
            auto character =
                static_cast<std::uint32_t>(node.span.end.column - node.member.length());
            auto length = static_cast<std::uint32_t>(node.member.length());
            add_token(line, character, length, SemanticKind::Type::Property);
        }
    }

    void visit(QualifiedAccessExpression&) override {}

    void visit(AssignExpression& node) override {
        visit_child(node.target);
        visit_child(node.value);
    }

    void visit(StructLiteralExpression& node) override {
        for (auto* field : node.fields) {
            visit_child(field);
        }
    }

    void visit(EnumVariantExpression&) override {}

    void visit(IfExpression& node) override {
        visit_child(node.condition);
        visit_child(node.then_branch);
        visit_child(node.else_branch);
    }

    void visit(WhenExpression& node) override {
        visit_child(node.subject);
        for (auto* arm : node.arms) {
            visit_child(arm);
        }
    }

    void visit(ClosureExpression& node) override {
        for (auto* parameter : node.parameters) {
            visit_child(parameter);
        }
        visit_child(node.body);
    }

    void visit(BlockExpression& node) override { visit_child(node.body); }

    void visit(BuiltinCall& node) override {
        for (auto* argument : node.arguments) {
            visit_child(argument);
        }
    }

    void visit(ArrayLiteralExpression& node) override {
        for (auto* element : node.elements) {
            visit_child(element);
        }
    }

    void visit(BlockStatement& node) override {
        for (auto* stmt : node.statements) {
            visit_child(stmt);
        }
    }

    void visit(ExpressionStatement& node) override { visit_child(node.expression); }

    void visit(ReturnStatement& node) override { visit_child(node.value); }

    void visit(WhileStatement& node) override {
        visit_child(node.condition);
        visit_child(node.body);
    }

    void visit(ForStatement& node) override {
        visit_child(node.initializer);
        visit_child(node.condition);
        visit_child(node.step);
        visit_child(node.body);
    }

    void visit(DeferStatement& node) override { visit_child(node.body); }

    void visit(InterfaceDeclaration& node) override {
        for (auto* method : node.methods) {
            visit_child(method);
        }
    }

    void visit(StructDeclaration& node) override {
        if (node.span.start.line > 0 && !node.name.empty()) {
            auto line = static_cast<std::uint32_t>(node.span.start.line - 1);
            auto character = static_cast<std::uint32_t>(node.span.start.column - 1) + 7U;
            auto length = static_cast<std::uint32_t>(node.name.length());
            add_token(line, character, length, SemanticKind::Type::Struct,
                      SemanticModifier::Declaration);
        }
        for (auto* field : node.fields) {
            visit_child(field);
        }
        for (auto* method : node.methods) {
            visit_child(method);
        }
    }

    void visit(EnumDeclaration& node) override {
        if (node.span.start.line > 0 && !node.name.empty()) {
            auto line = static_cast<std::uint32_t>(node.span.start.line - 1);
            auto character = static_cast<std::uint32_t>(node.span.start.column - 1) + 5U;
            auto length = static_cast<std::uint32_t>(node.name.length());
            add_token(line, character, length, SemanticKind::Type::Enum,
                      SemanticModifier::Declaration);
        }
        for (auto* variant : node.variants) {
            visit_child(variant);
        }
    }

    void visit(VarDeclaration& node) override {
        if (node.span.start.line > 0 && !node.name.empty()) {
            auto prefix_len = (node.storage_kind == StorageKind::Type::VarMut) ? 8U : 4U;
            auto line = static_cast<std::uint32_t>(node.span.start.line - 1);
            auto character = static_cast<std::uint32_t>(node.span.start.column - 1) + prefix_len;
            auto length = static_cast<std::uint32_t>(node.name.length());
            add_token(line, character, length, SemanticKind::Type::Variable,
                      SemanticModifier::Declaration);
        }
        visit_child(node.annotation);
        visit_child(node.initializer);
    }

    void visit(ExternFunctionDeclaration& node) override { visit_child(node.prototype); }

    void visit(ExternVarDeclaration& node) override { visit_child(node.annotation); }

    void visit(ImportStatement&) override {}

    void visit(TypeAliasDeclaration& node) override { visit_child(node.target); }
};

export class SyntaxIndex {
  private:
    std::vector<Node*> indexed_nodes;
    std::vector<SemanticToken> semantic_tokens;

    static bool contains(Span span, Position position) {
        if (span.start.line == 0 || position.line < span.start.line || position.line > span.end.line) {
            return false;
        }

        if (position.line == span.start.line && position.column < span.start.column) {
            return false;
        }

        return position.line != span.end.line || position.column <= span.end.column;
    }

  public:
    SyntaxIndex(std::string_view content, const Program* program) {
        std::vector<RawToken> tokens;
        tokens.reserve(256);

        std::map<std::pair<std::uint32_t, std::uint32_t>, RawToken> semantic_map;
        if (program != nullptr) {
            AstTokenCollector collector;
            for (auto* statement : program->statements) {
                collector.visit_child(statement);
            }
            indexed_nodes = std::move(collector.nodes);
            for (auto& token : collector.tokens) {
                semantic_map.emplace(std::make_pair(token.line, token.character), token);
            }
        }

        Lexer lexer(content);
        while (true) {
            auto token = lexer.next_token();
            if (token.type == Token::Type::Eof) {
                break;
            }

            if (token.span.start.line == 0) {
                continue;
            }

            auto line = static_cast<std::uint32_t>(token.span.start.line - 1);
            auto character = static_cast<std::uint32_t>(token.span.start.column - 1);
            auto length = static_cast<std::uint32_t>(token.value.length());
            if (length == 0) {
                length =
                    static_cast<std::uint32_t>(token.span.end.column - token.span.start.column + 1);
            }

            if (token.type == Token::Type::Identifier) {
                auto key = std::make_pair(line, character);
                if (auto it = semantic_map.find(key); it != semantic_map.end()) {
                    tokens.push_back(it->second);
                } else {
                    tokens.emplace_back(line, character, length, SemanticKind::Type::Variable);
                }
                continue;
            }

            switch (token.type) {
            case Token::Type::Fn:
            case Token::Type::Var:
            case Token::Type::Const:
            case Token::Type::Struct:
            case Token::Type::Enum:
            case Token::Type::Import:
            case Token::Type::Interface:
            case Token::Type::Return:
            case Token::Type::If:
            case Token::Type::Else:
            case Token::Type::When:
            case Token::Type::For:
            case Token::Type::In:
            case Token::Type::While:
            case Token::Type::Type:
            case Token::Type::Defer:
            case Token::Type::Do:
            case Token::Type::Is:
            case Token::Type::As:
                tokens.emplace_back(line, character, length, SemanticKind::Type::Keyword);
                break;

            case Token::Type::Mut:
            case Token::Type::Static:
            case Token::Type::Public:
            case Token::Type::Private:
            case Token::Type::Extern:
            case Token::Type::Override:
                tokens.emplace_back(line, character, length, SemanticKind::Type::Modifier);
                break;

            case Token::Type::Number:
            case Token::Type::Float:
                tokens.emplace_back(line, character, length, SemanticKind::Type::Number);
                break;

            case Token::Type::String:
            case Token::Type::Char:
                tokens.emplace_back(line, character, length, SemanticKind::Type::String);
                break;

            case Token::Type::Boolean:
            case Token::Type::Null:
                tokens.emplace_back(line, character, length, SemanticKind::Type::Keyword);
                break;

            case Token::Type::Plus:
            case Token::Type::Minus:
            case Token::Type::Asterisk:
            case Token::Type::Divide:
            case Token::Type::Modulo:
            case Token::Type::Equals:
            case Token::Type::NotEquals:
            case Token::Type::LessEqual:
            case Token::Type::GreaterEqual:
            case Token::Type::LessThan:
            case Token::Type::GreaterThan:
            case Token::Type::Assign:
            case Token::Type::Arrow:
            case Token::Type::And:
            case Token::Type::Or:
            case Token::Type::Not:
                tokens.emplace_back(line, character, length, SemanticKind::Type::Operator);
                break;

            default:
                break;
            }
        }

        std::ranges::sort(tokens, [](const RawToken& a, const RawToken& b) {
            if (a.line != b.line) {
                return a.line < b.line;
            }
            return a.character < b.character;
        });

        std::vector<RawToken> deduped;
        deduped.reserve(tokens.size());
        for (const auto& token : tokens) {
            if (!deduped.empty()) {
                const auto& last = deduped.back();
                if (last.line == token.line && token.character < last.character + last.length) {
                    continue;
                }
            }
            deduped.push_back(token);
        }

        semantic_tokens.reserve(deduped.size());
        for (const auto& token : deduped) {
            auto start = Position(token.line + 1, token.character + 1);
            auto end = Position(token.line + 1, token.character + token.length);
            semantic_tokens.emplace_back(Span(start, end), token.kind, token.modifiers);
        }
    }

    Node* node(Position position) const {
        Node* result = nullptr;
        for (auto* node : indexed_nodes) {
            if (!contains(node->span, position)) {
                continue;
            }

            if (result == nullptr ||
                (node->span.start.line >= result->span.start.line &&
                 node->span.end.line <= result->span.end.line)) {
                result = node;
            }
        }

        return result;
    }

    const Scope* scope(Position position) const {
        const Scope* result = nullptr;
        Position latest;

        for (auto* node : indexed_nodes) {
            if (auto* block = node->as<BlockStatement>();
                block != nullptr && block->scope != nullptr && contains(block->span, position)) {
                auto starts_after_latest = block->span.start.line > latest.line ||
                                           (block->span.start.line == latest.line &&
                                            block->span.start.column >= latest.column);
                if (starts_after_latest) {
                    result = block->scope;
                    latest = block->span.start;
                }
                continue;
            }

            auto* expression = dynamic_cast<Expression*>(node);
            if (expression == nullptr || expression->scope == nullptr) {
                continue;
            }

            auto starts_before = expression->span.start.line < position.line ||
                                 (expression->span.start.line == position.line &&
                                  expression->span.start.column <= position.column);
            auto starts_after_latest = expression->span.start.line > latest.line ||
                                       (expression->span.start.line == latest.line &&
                                        expression->span.start.column >= latest.column);
            if (starts_before && starts_after_latest) {
                result = expression->scope;
                latest = expression->span.start;
            }
        }

        return result;
    }

    const std::vector<Node*>& nodes() const { return indexed_nodes; }

    const std::vector<SemanticToken>& tokens() const { return semantic_tokens; }

    std::vector<SemanticToken> tokens(Span range) const {
        std::vector<SemanticToken> result;
        result.reserve(semantic_tokens.size());
        for (const auto& token : semantic_tokens) {
            if (token.span.end.line < range.start.line || token.span.start.line > range.end.line) {
                continue;
            }

            result.push_back(token);
        }
        return result;
    }
};
