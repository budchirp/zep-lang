module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.lsp.analysis.index;

import zep.common.source.position;
import zep.common.source.span;
import zep.frontend.lexer;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.kind;
import zep.frontend.sema.symbols;
import zep.frontend.sema.scopes.scope;
import zep.frontend.token;
import zep.frontend.token.keywords;
import zep.lsp.analysis.types;
import zep.lsp.analysis.symbols;

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
  private:
    const std::vector<Token>& lexemes;
    Node* current;

    void add_symbol(Node& node, SemanticKind::Type kind,
                    std::uint32_t modifiers = SemanticModifier::Declaration) {
        const auto selected = Symbols::selection(node, lexemes);

        if (selected.start.line == 0 || selected.start.line != selected.end.line) {
            return;
        }

        add_token(static_cast<std::uint32_t>(selected.start.line - 1),
                  static_cast<std::uint32_t>(selected.start.column - 1),
                  static_cast<std::uint32_t>(selected.end.column - selected.start.column),
                  kind, modifiers);
    }

  public:
    std::vector<RawToken> tokens;
    std::vector<Node*> nodes;
    std::vector<CatchArm*> bindings;
    std::unordered_map<Node*, Span> ranges;

    explicit AstTokenCollector(const std::vector<Token>& lexemes)
        : lexemes(lexemes), current(nullptr), tokens(), nodes(), bindings(), ranges() {
        tokens.reserve(256);
        nodes.reserve(256);
        bindings.reserve(8);
        ranges.reserve(256);
    }

    void add_token(std::uint32_t line, std::uint32_t character, std::uint32_t length,
                   SemanticKind::Type kind, std::uint32_t modifiers = 0) {
        tokens.emplace_back(line, character, length, kind, modifiers);
    }

    void visit_child(Node* child) {
        if (child != nullptr) {
            auto* parent = current;
            current = child;

            if (ranges.emplace(child, child->span).second) {
                nodes.push_back(child);

                visit_node(*child);
            }

            current = parent;

            if (parent != nullptr) {
                ranges.at(parent) = Symbols::cover(ranges.at(parent), ranges.at(child));
            }
        }
    }

    void visit(TypeExpression& node) override {
        for (auto* error : node.errors) {
            visit_child(error);
        }

        visit_child(node.element);
        visit_child(node.return_type);

        for (auto* parameter : node.parameters) {
            visit_child(parameter);
        }

        for (auto* argument : node.generic_arguments) {
            visit_child(argument);
        }

        for (auto* size : node.array_sizes) {
            visit_child(size);
        }

        if (node.span.start.line > 0 && node.type != nullptr && node.span.start.line == node.span.end.line) {
            auto line = static_cast<std::uint32_t>(node.span.start.line - 1);
            auto character = static_cast<std::uint32_t>(node.span.start.column - 1);
            auto length =
                static_cast<std::uint32_t>(node.span.end.column - node.span.start.column);
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
        add_symbol(node, SemanticKind::Type::Parameter);

        visit_child(node.type);
    }

    void visit(Argument& node) override { visit_child(node.value); }

    void visit(TestDeclaration& node) override {
        visit_child(node.function->body);
    }

    void visit(FunctionDeclaration& node) override {
        add_symbol(node, SemanticKind::Type::Function,
                    SemanticModifier::Declaration | SemanticModifier::Definition);

        visit_child(node.prototype);
        visit_child(node.body);
    }

    void visit(FunctionPrototype& node) override {
        for (auto* parameter : node.generic_parameters) {
            visit_child(parameter);
        }

        for (auto* error : node.errors) {
            visit_child(error);
        }

        for (auto* parameter : node.parameters) {
            visit_child(parameter);
        }
        visit_child(node.return_type);
    }

    void visit(Field& node) override {
        add_symbol(node, SemanticKind::Type::Property);

        visit_child(node.type);
        visit_child(node.default_value);
    }

    void visit(EnumVariant& node) override {
        add_symbol(node, SemanticKind::Type::EnumMember);

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
        visit_child(node.callee);

        if (auto* id = node.callee->as<IdentifierExpression>(); id != nullptr) {
            if (id->span.start.line > 0 && !id->name.empty()) {
                auto line = static_cast<std::uint32_t>(id->span.start.line - 1);
                auto character = static_cast<std::uint32_t>(id->span.start.column - 1);
                auto length = static_cast<std::uint32_t>(id->name.length());
                add_token(line, character, length, SemanticKind::Type::Function);
            }
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

        add_symbol(node, SemanticKind::Type::Property, SemanticModifier::None);
    }

    void visit(QualifiedAccessExpression&) override {}

    void visit(NewExpression& node) override {
        visit_child(node.storage);
        visit_child(node.value);
    }

    void visit(TryExpression& node) override { visit_child(node.value); }

    void visit(ThrowExpression& node) override { visit_child(node.value); }

    void visit(CatchExpression& node) override {
        visit_child(node.value);

        for (auto& arm : node.arms) {
            bindings.push_back(&arm);
            if (arm.span.start.line > 0) {
                add_token(static_cast<std::uint32_t>(arm.span.start.line - 1),
                    static_cast<std::uint32_t>(arm.span.start.column - 1),
                    static_cast<std::uint32_t>(arm.name.size()), SemanticKind::Type::Variable,
                    SemanticModifier::Declaration);
            }

            visit_child(arm.annotation);
            visit_child(arm.body);
        }
    }

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
        for (auto* error : node.errors) {
            visit_child(error);
        }

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
        add_symbol(node, SemanticKind::Type::Interface);

        for (auto* parent : node.inheritance) {
            visit_child(parent);
        }

        for (auto* method : node.methods) {
            add_symbol(*method, SemanticKind::Type::Method);
            visit_child(method);
        }
    }

    void visit(StructDeclaration& node) override {
        add_symbol(node, SemanticKind::Type::Struct);

        for (auto* parent : node.inheritance) {
            visit_child(parent);
        }

        for (auto* field : node.fields) {
            visit_child(field);
        }
        for (auto* method : node.methods) {
            visit_child(method);
        }

        for (auto* structure : node.structs) {
            visit_child(structure);
        }

        for (auto* enumeration : node.enums) {
            visit_child(enumeration);
        }
    }

    void visit(EnumDeclaration& node) override {
        add_symbol(node, SemanticKind::Type::Enum);

        visit_child(node.backing_type);

        for (auto* parent : node.inheritance) {
            visit_child(parent);
        }

        for (auto* field : node.fields) {
            visit_child(field);
        }

        for (auto* variant : node.variants) {
            visit_child(variant);
        }

        for (auto* method : node.methods) {
            visit_child(method);
        }
    }

    void visit(VarDeclaration& node) override {
        add_symbol(node, SemanticKind::Type::Variable);

        visit_child(node.annotation);
        visit_child(node.initializer);
    }

    void visit(ExternFunctionDeclaration& node) override {
        add_symbol(node, SemanticKind::Type::Function);

        visit_child(node.prototype);
    }

    void visit(ExternVarDeclaration& node) override {
        add_symbol(node, SemanticKind::Type::Variable);

        visit_child(node.annotation);
    }

    void visit(ImportStatement&) override {}

    void visit(TypeAliasDeclaration& node) override {
        add_symbol(node, SemanticKind::Type::Type);

        visit_child(node.target);
    }
};

export class SyntaxIndex {
  private:
    std::vector<Node*> indexed_nodes;
    std::vector<SemanticToken> semantic_tokens;

  public:
    std::vector<Token> lexemes;
    std::unordered_map<Node*, Span> ranges;
    std::vector<CatchArm*> bindings;

    SyntaxIndex(std::string_view content, const Program* program) {
        Lexer lexer(content);
        lexemes.reserve(256);

        while (true) {
            const auto token = lexer.next_token();

            if (token.type == Token::Type::Eof) {
                break;
            }

            lexemes.push_back(token);
        }

        std::vector<RawToken> tokens;
        tokens.reserve(256);

        std::map<std::pair<std::uint32_t, std::uint32_t>, RawToken> semantic_map;
        if (program != nullptr) {
            AstTokenCollector collector(lexemes);
            for (auto* statement : program->statements) {
                collector.visit_child(statement);
            }
            indexed_nodes = std::move(collector.nodes);
            ranges = std::move(collector.ranges);
            bindings = std::move(collector.bindings);
            for (auto& token : collector.tokens) {
                semantic_map.emplace(std::make_pair(token.line, token.character), token);
            }
        }

        for (const auto& token : lexemes) {
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

                if (token.value == "throws" && lexemes.size() > 1 &&
                    lexemes[lexemes.size() - 2].type == Token::Type::At) {
                    tokens.emplace_back(line, character, length, SemanticKind::Type::Modifier);
                } else if (auto it = semantic_map.find(key); it != semantic_map.end()) {
                    tokens.emplace_back(line, character, length, it->second.kind, it->second.modifiers);
                } else if (token.value == "move" || token.value == "default" || token.value == "operator") {
                    tokens.emplace_back(line, character, length, SemanticKind::Type::Keyword);
                } else {
                    tokens.emplace_back(line, character, length, SemanticKind::Type::Variable);
                }
                continue;
            }

            switch (token.type) {
            case Token::Type::Fn:
            case Token::Type::Test:
            case Token::Type::Var:
            case Token::Type::Const:
            case Token::Type::Struct:
            case Token::Type::Enum:
            case Token::Type::Import:
            case Token::Type::Interface:
            case Token::Type::Return:
            case Token::Type::Try:
            case Token::Type::Catch:
            case Token::Type::Throw:
            case Token::Type::If:
            case Token::Type::Else:
            case Token::Type::When:
            case Token::Type::For:
            case Token::Type::While:
            case Token::Type::Type:
            case Token::Type::Defer:
            case Token::Type::Do:
            case Token::Type::Is:
            case Token::Type::As:
            case Token::Type::New:
            case Token::Type::Delete:
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

        for (auto* node : indexed_nodes) {
            const auto* call = node->as<CallExpression>();

            if (call == nullptr) {
                continue;
            }

            auto depth = std::size_t{0};

            for (const auto& token : lexemes) {
                if (std::pair(token.span.start.line, token.span.start.column) <
                    std::pair(call->callee->span.end.line, call->callee->span.end.column)) {
                    continue;
                }

                if (token.type == Token::Type::LeftParen) {
                    ++depth;
                } else if (token.type == Token::Type::RightParen && depth > 0 && --depth == 0) {
                    ranges.at(node) = Symbols::cover(ranges.at(node), token.span);
                    break;
                }
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
            const auto extent = range(*node);

            if (!extent.contains(position)) {
                continue;
            }

            if (result == nullptr ||
                (std::pair(extent.start.line, extent.start.column) >=
                    std::pair(range(*result).start.line, range(*result).start.column) &&
                 std::pair(extent.end.line, extent.end.column) <=
                    std::pair(range(*result).end.line, range(*result).end.column))) {
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
                block != nullptr && block->scope != nullptr && range(*block).contains(position)) {
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
            if (expression == nullptr || expression->scope == nullptr || !range(*expression).contains(position)) {
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

    Span range(Node& node) const {
        const auto found = ranges.find(&node);

        return found != ranges.end() ? found->second : node.span;
    }

    Span selection(Node& node) const {
        return Symbols::selection(node, lexemes);
    }

    const CatchArm* binding(Position position) const {
        for (const auto* arm : bindings) {
            if (arm->span.contains(position)) {
                return arm;
            }
        }

        return nullptr;
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
