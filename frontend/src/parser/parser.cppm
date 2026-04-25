module;

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.parser;

import zep.common.span;
import zep.frontend.ast;
import zep.frontend.ast.program;
import zep.frontend.arena;
import zep.frontend.arena;
import zep.frontend.token;
import zep.frontend.lexer;
import zep.frontend.parser.precedence;
import zep.common.logger;
import zep.frontend.sema.context;
import zep.frontend.sema.type;
import zep.frontend.sema.kind;

export class Parser {
  private:
    Context& context;

    Lexer lexer;

    const Logger& logger;

    // ---

    Token current_token;
    Token peek_token;

    // ---

    void advance() {
        current_token = peek_token;
        peek_token = lexer.next_token();
    }

    [[nodiscard]] bool check(Token::Type type) const { return current_token.type == type; }

    [[nodiscard]] bool peek(Token::Type type) const { return peek_token.type == type; }

    [[nodiscard]] bool match(Token::Type type) {
        if (check(type)) {
            advance();

            return true;
        }

        return false;
    }

    Token expect(Token::Type type) {
        if (!check(type)) {
            logger.error(current_token.span,
                         "unexpected token '" + std::string(current_token.value) + "'");
        }

        auto token = current_token;

        advance();

        return token;
    }

    // ---

    Visibility::Type parse_visibility(bool mandatory = true) {
        switch (current_token.type) {
        case Token::Type::Public:
            advance();

            return Visibility::Type::Public;
        case Token::Type::Private:
            advance();

            return Visibility::Type::Private;
        default:
            if (mandatory) {
                logger.error(current_token.span, "expected 'public' or 'private'");
            }

            return Visibility::Type::Public;
        }
    }

    const Type* resolve_primitive(const std::string& name) {
        if (name == "void") {
            return context.env.primitives["void"];
        }

        if (name == "string") {
            return context.env.primitives["string"];
        }

        if (name == "boolean") {
            return context.env.primitives["boolean"];
        }

        if (name.size() >= 2 && (name[0] == 'i' || name[0] == 'u')) {
            return context.env.primitives[name[0] + name.substr(1)];
        }

        if (name.size() >= 2 && name[0] == 'f') {
            return context.env.primitives["f" + name.substr(1)];
        }

        return context.types.create<NamedType>(name, std::vector<GenericArgumentType>{});
    }

    Statement* parse_statement() {
        auto visibility = parse_visibility(false);

        switch (current_token.type) {
        case Token::Type::Import:
            return parse_import_statement();
        case Token::Type::Extern:
            return parse_extern_declaration(visibility);
        case Token::Type::Fn:
            return parse_function_declaration(visibility);
        case Token::Type::Struct:
            return parse_struct_declaration(visibility);
        case Token::Type::Var:
            return parse_var_declaration(visibility);
        case Token::Type::Return:
            return parse_return_statement();
        case Token::Type::LeftBrace:
            return parse_block_statement();
        default: {
            return context.nodes.create<ExpressionStatement>(parse_expression());
        }
        }
    }

    Expression* parse_expression(Precedence::Type precedence = Precedence::Type::None) {
        auto* left = parse_prefix();
        while (Precedence::get(current_token.type) > precedence) {
            left = parse_infix(left);
        }

        return left;
    }

    Expression* parse_primary() {
        auto span = current_token.span;

        switch (current_token.type) {

        case Token::Type::String: {
            auto value = std::string(current_token.value);
            advance();

            return context.nodes.create<StringLiteral>(span, std::move(value));
        }
        case Token::Type::Number: {
            auto value = std::string(current_token.value);
            advance();

            return context.nodes.create<NumberLiteral>(span, std::move(value));
        }
        case Token::Type::Float: {
            auto value = std::string(current_token.value);
            advance();

            return context.nodes.create<FloatLiteral>(span, std::move(value));
        }
        case Token::Type::Boolean: {
            auto value = current_token.value == "true";
            advance();

            return context.nodes.create<BooleanLiteral>(span, value);
        }
        case Token::Type::Identifier: {
            auto name = std::string(current_token.value);
            advance();

            if (check(Token::Type::LessThan)) {
                auto generic_arguments = parse_generic_arguments();

                if (check(Token::Type::LeftParen)) {
                    auto* callee =
                        context.nodes.create<IdentifierExpression>(span, std::move(name));
                    return parse_call_expression(callee, std::move(generic_arguments));
                }

                if (check(Token::Type::LeftBrace)) {
                    auto* type_name =
                        context.nodes.create<IdentifierExpression>(span, std::move(name));
                    return parse_struct_literal(type_name, std::move(generic_arguments));
                }

                logger.error(current_token.span, "expected '(' or '{' after generic arguments");
            }

            if (check(Token::Type::LeftBrace)) {
                auto* type_name = context.nodes.create<IdentifierExpression>(span, std::move(name));
                return parse_struct_literal(type_name, {});
            }

            return context.nodes.create<IdentifierExpression>(span, std::move(name));
        }
        case Token::Type::LeftParen: {
            advance();

            auto* expression = parse_expression();
            expect(Token::Type::RightParen);

            return expression;
        }
        case Token::Type::If: {
            return parse_if_expression();
        }
        default:
            logger.error(span, "unexpected token '" + std::string(current_token.value) +
                                   "' in expression");
        }
    }

    Expression* parse_infix(Expression* left) {
        switch (current_token.type) {
        case Token::Type::Plus:
        case Token::Type::Minus:
        case Token::Type::Asterisk:
        case Token::Type::Divide:
        case Token::Type::Modulo:
        case Token::Type::Equals:
        case Token::Type::NotEquals:
        case Token::Type::LessThan:
        case Token::Type::GreaterThan:
        case Token::Type::LessEqual:
        case Token::Type::GreaterEqual:
        case Token::Type::And:
        case Token::Type::Or:
        case Token::Type::As:
        case Token::Type::Is:
            return parse_binary_expression(left);
        case Token::Type::Assign: {
            auto span = current_token.span;

            advance();

            auto* right = parse_expression(
                static_cast<Precedence::Type>(static_cast<int>(Precedence::Type::Assignment) - 1));
            return context.nodes.create<AssignExpression>(span, left, right);
        }
        case Token::Type::LeftParen:
        case Token::Type::LeftBracket:
        case Token::Type::Dot:
            return parse_postfix(left);
        default:
            logger.error(current_token.span,
                         "unexpected token '" + std::string(current_token.value) + "'");
        }
    }

    Expression* parse_prefix() {
        switch (current_token.type) {
        case Token::Type::Minus:
        case Token::Type::Plus:
        case Token::Type::Not:
        case Token::Type::Asterisk:
        case Token::Type::Ampersand:
            return parse_unary_expression();
        default:
            return parse_primary();
        }
    }

    Expression* parse_postfix(Expression* left) {
        switch (current_token.type) {
        case Token::Type::LeftParen:
            return parse_call_expression(left);
        case Token::Type::LeftBracket:
            return parse_index_expression(left);
        case Token::Type::Dot:
            return parse_member_expression(left);
        default:
            logger.error(current_token.span,
                         "unexpected token '" + std::string(current_token.value) + "'");
        }
    }

    // ---

    std::vector<Parameter*> parse_parameters() {
        auto parameters = std::vector<Parameter*>{};

        while (!check(Token::Type::RightParen) && !check(Token::Type::Eof)) {
            auto span = current_token.span;

            auto is_variadic = false;
            if (match(Token::Type::Ellipsis)) {
                is_variadic = true;
            }

            auto name = std::string(expect(Token::Type::Identifier).value);
            expect(Token::Type::Colon);
            auto* type_expression = parse_type_expression();

            parameters.push_back(context.nodes.create<Parameter>(span, is_variadic, std::move(name),
                                                                 type_expression));

            if (!check(Token::Type::RightParen)) {
                expect(Token::Type::Comma);
            }
        }

        return parameters;
    }

    std::vector<Argument*> parse_arguments() {
        auto arguments = std::vector<Argument*>{};

        while (!check(Token::Type::RightParen) && !check(Token::Type::Eof)) {
            auto span = current_token.span;
            auto* value = parse_expression();

            arguments.push_back(context.nodes.create<Argument>(span, "", value));

            if (!check(Token::Type::RightParen)) {
                expect(Token::Type::Comma);
            }
        }

        return arguments;
    }

    std::vector<GenericParameter*> parse_generic_parameters() {
        auto generic_parameters = std::vector<GenericParameter*>{};

        expect(Token::Type::LessThan);
        while (!check(Token::Type::GreaterThan) && !check(Token::Type::Eof)) {
            auto span = current_token.span;

            auto name = std::string(expect(Token::Type::Identifier).value);

            auto* constraint = static_cast<TypeExpression*>(nullptr);
            if (match(Token::Type::Colon)) {
                constraint = parse_type_expression();
            }

            generic_parameters.push_back(
                context.nodes.create<GenericParameter>(span, std::move(name), constraint));

            if (!check(Token::Type::GreaterThan)) {
                expect(Token::Type::Comma);
            }
        }
        expect(Token::Type::GreaterThan);

        return generic_parameters;
    }

    std::vector<GenericArgument*> parse_generic_arguments() {
        auto generic_arguments = std::vector<GenericArgument*>{};

        expect(Token::Type::LessThan);
        while (!check(Token::Type::GreaterThan) && !check(Token::Type::Eof)) {
            auto span = current_token.span;

            auto* type_expression = parse_type_expression();

            generic_arguments.push_back(
                context.nodes.create<GenericArgument>(span, "", type_expression));

            if (!check(Token::Type::GreaterThan)) {
                expect(Token::Type::Comma);
            }
        }
        expect(Token::Type::GreaterThan);

        return generic_arguments;
    }

    // ---

    TypeExpression* parse_type_expression() {
        auto span = current_token.span;

        if (check(Token::Type::Asterisk)) {
            advance();

            auto is_mutable = false;
            if (check(Token::Type::Mut)) {
                is_mutable = true;
                advance();
            }

            auto* element = parse_type_expression();

            const auto* pointer_type = context.types.create<PointerType>(element->type, is_mutable);
            return context.nodes.create<TypeExpression>(span, pointer_type);
        }

        auto name = std::string(expect(Token::Type::Identifier).value);
        const auto* type = resolve_primitive(name);

        if (check(Token::Type::LessThan)) {
            if (type->kind == Type::Kind::Type::Named) {
                auto generic_arguments = parse_generic_arguments();

                auto generic_argument_types = std::vector<GenericArgumentType>{};
                generic_argument_types.reserve(generic_arguments.size());

                for (auto* generic_argument : generic_arguments) {
                    generic_argument_types.emplace_back("", generic_argument->type->type);
                }

                type = context.types.create<NamedType>(name, std::move(generic_argument_types));
            }
        }

        while (check(Token::Type::LeftBracket)) {
            advance();

            auto size = std::optional<std::size_t>{};
            if (check(Token::Type::Number)) {
                size = std::stoull(std::string(current_token.value));
                advance();
            }

            expect(Token::Type::RightBracket);

            type = context.types.create<ArrayType>(type, size);
        }

        return context.nodes.create<TypeExpression>(span, type);
    }

    Expression* parse_unary_expression() {
        auto span = current_token.span;

        auto op = UnaryExpression::Operator::Type::Minus;

        switch (current_token.type) {
        case Token::Type::Minus:
            op = UnaryExpression::Operator::Type::Minus;
            break;
        case Token::Type::Plus:
            op = UnaryExpression::Operator::Type::Plus;
            break;
        case Token::Type::Not:
            op = UnaryExpression::Operator::Type::Not;
            break;
        case Token::Type::Asterisk:
            op = UnaryExpression::Operator::Type::Dereference;
            break;
        case Token::Type::Ampersand:
            op = UnaryExpression::Operator::Type::AddressOf;
            break;
        default:
            logger.error(span, "unexpected token '" + std::string(current_token.value) +
                                   "' in unary expression");
        }

        advance();

        auto* operand = parse_expression(Precedence::Type::Unary);
        return context.nodes.create<UnaryExpression>(span, op, operand);
    }

    Expression* parse_binary_expression(Expression* left) {
        auto span = current_token.span;

        auto op = BinaryExpression::Operator::Type::Plus;

        switch (current_token.type) {
        case Token::Type::Plus:
            op = BinaryExpression::Operator::Type::Plus;
            break;
        case Token::Type::Minus:
            op = BinaryExpression::Operator::Type::Minus;
            break;
        case Token::Type::Asterisk:
            op = BinaryExpression::Operator::Type::Asterisk;
            break;
        case Token::Type::Divide:
            op = BinaryExpression::Operator::Type::Divide;
            break;
        case Token::Type::Modulo:
            op = BinaryExpression::Operator::Type::Modulo;
            break;
        case Token::Type::Equals:
            op = BinaryExpression::Operator::Type::Equals;
            break;
        case Token::Type::NotEquals:
            op = BinaryExpression::Operator::Type::NotEquals;
            break;
        case Token::Type::LessThan:
            op = BinaryExpression::Operator::Type::LessThan;
            break;
        case Token::Type::GreaterThan:
            op = BinaryExpression::Operator::Type::GreaterThan;
            break;
        case Token::Type::LessEqual:
            op = BinaryExpression::Operator::Type::LessEqual;
            break;
        case Token::Type::GreaterEqual:
            op = BinaryExpression::Operator::Type::GreaterEqual;
            break;
        case Token::Type::And:
            op = BinaryExpression::Operator::Type::And;
            break;
        case Token::Type::Or:
            op = BinaryExpression::Operator::Type::Or;
            break;
        case Token::Type::As:
            op = BinaryExpression::Operator::Type::As;
            break;
        case Token::Type::Is:
            op = BinaryExpression::Operator::Type::Is;
            break;
        default:
            logger.error(span, "unexpected token '" + std::string(current_token.value) +
                                   "' in binary expression");
        }

        auto precedence = Precedence::get(current_token.type);

        advance();

        if (op == BinaryExpression::Operator::Type::As ||
            op == BinaryExpression::Operator::Type::Is) {
            auto* right = parse_type_expression();
            return context.nodes.create<BinaryExpression>(span, left, op, right);
        }

        auto* right = parse_expression(precedence);
        return context.nodes.create<BinaryExpression>(span, left, op, right);
    }

    IfExpression* parse_if_expression() {
        auto span = current_token.span;

        expect(Token::Type::If);

        expect(Token::Type::LeftParen);
        auto* condition = parse_expression();
        expect(Token::Type::RightParen);

        auto* then_branch = parse_block_statement();

        auto* else_branch = static_cast<Statement*>(nullptr);
        if (match(Token::Type::Else)) {
            else_branch = parse_block_statement();
        }

        return context.nodes.create<IfExpression>(span, condition, then_branch, else_branch);
    }

    StructLiteralExpression*
    parse_struct_literal(IdentifierExpression* name,
                         std::vector<GenericArgument*> generic_arguments = {}) {
        auto span = name->span;

        expect(Token::Type::LeftBrace);

        auto fields = std::vector<StructLiteralField*>{};
        while (!check(Token::Type::RightBrace) && !check(Token::Type::Eof)) {
            auto field_span = current_token.span;

            auto field_name = std::string(expect(Token::Type::Identifier).value);
            expect(Token::Type::Colon);
            auto* value = parse_expression();

            fields.push_back(
                context.nodes.create<StructLiteralField>(field_span, std::move(field_name), value));

            if (!check(Token::Type::RightBrace)) {
                expect(Token::Type::Comma);
            }
        }

        expect(Token::Type::RightBrace);

        return context.nodes.create<StructLiteralExpression>(
            span, name, std::move(generic_arguments), std::move(fields));
    }

    CallExpression* parse_call_expression(Expression* callee,
                                          std::vector<GenericArgument*> generic_arguments = {}) {
        auto span = current_token.span;

        expect(Token::Type::LeftParen);
        auto arguments = parse_arguments();
        expect(Token::Type::RightParen);

        return context.nodes.create<CallExpression>(span, callee, std::move(generic_arguments),
                                                    std::move(arguments));
    }

    IndexExpression* parse_index_expression(Expression* value) {
        auto span = current_token.span;

        expect(Token::Type::LeftBracket);
        auto* index = parse_expression();
        expect(Token::Type::RightBracket);

        return context.nodes.create<IndexExpression>(span, value, index);
    }

    MemberExpression* parse_member_expression(Expression* value) {
        auto span = current_token.span;

        expect(Token::Type::Dot);
        auto member = std::string(expect(Token::Type::Identifier).value);

        return context.nodes.create<MemberExpression>(span, value, std::move(member));
    }

    ImportStatement* parse_import_statement() {
        auto span = current_token.span;

        expect(Token::Type::Import);

        auto path = std::vector<IdentifierExpression*>{};

        auto first = expect(Token::Type::Identifier);
        path.push_back(
            context.nodes.create<IdentifierExpression>(first.span, std::string(first.value)));

        while (match(Token::Type::Dot)) {
            auto part = expect(Token::Type::Identifier);
            path.push_back(
                context.nodes.create<IdentifierExpression>(part.span, std::string(part.value)));
        }

        return context.nodes.create<ImportStatement>(span, std::move(path));
    }

    StructDeclaration* parse_struct_declaration(Visibility::Type visibility) {
        auto span = current_token.span;

        expect(Token::Type::Struct);

        auto name = std::string(expect(Token::Type::Identifier).value);

        auto generic_parameters = std::vector<GenericParameter*>{};
        if (check(Token::Type::LessThan)) {
            generic_parameters = parse_generic_parameters();
        }

        expect(Token::Type::LeftBrace);

        auto fields = std::vector<StructField*>{};
        while (!check(Token::Type::RightBrace) && !check(Token::Type::Eof)) {
            auto field_span = current_token.span;

            auto field_visibility = Visibility::Type::Public;
            if (check(Token::Type::Public) || check(Token::Type::Private)) {
                field_visibility = parse_visibility();
            }

            auto field_name = std::string(expect(Token::Type::Identifier).value);
            expect(Token::Type::Colon);
            auto* type_expression = parse_type_expression();

            fields.push_back(context.nodes.create<StructField>(
                field_span, field_visibility, std::move(field_name), type_expression));
        }

        expect(Token::Type::RightBrace);

        return context.nodes.create<StructDeclaration>(
            span, visibility, std::move(name), std::move(generic_parameters), std::move(fields));
    }

    FunctionPrototype* parse_function_prototype() {
        auto span = current_token.span;

        auto name = std::string(expect(Token::Type::Identifier).value);

        auto generic_parameters = std::vector<GenericParameter*>{};
        if (check(Token::Type::LessThan)) {
            generic_parameters = parse_generic_parameters();
        }

        expect(Token::Type::LeftParen);
        auto parameters = parse_parameters();
        expect(Token::Type::RightParen);

        expect(Token::Type::Colon);
        auto* return_type = parse_type_expression();

        return context.nodes.create<FunctionPrototype>(span, std::move(name),
                                                       std::move(generic_parameters),
                                                       std::move(parameters), return_type);
    }

    FunctionDeclaration* parse_function_declaration(Visibility::Type visibility) {
        auto span = current_token.span;

        expect(Token::Type::Fn);

        auto* prototype = parse_function_prototype();
        auto* body = parse_block_statement();

        return context.nodes.create<FunctionDeclaration>(span, visibility, prototype, body);
    }

    VarDeclaration* parse_var_declaration(Visibility::Type visibility) {
        auto span = current_token.span;

        auto storage_kind = StorageKind::Type::Var;
        if (check(Token::Type::Const)) {
            storage_kind = StorageKind::Type::Const;
        }

        advance();

        if (storage_kind == StorageKind::Type::Var && match(Token::Type::Mut)) {
            storage_kind = StorageKind::Type::VarMut;
        }

        auto name = std::string(expect(Token::Type::Identifier).value);

        auto* type_expression = static_cast<TypeExpression*>(nullptr);
        if (match(Token::Type::Colon)) {
            type_expression = parse_type_expression();
        }

        auto* initializer = static_cast<Expression*>(nullptr);
        if (match(Token::Type::Assign)) {
            initializer = parse_expression();
        }

        return context.nodes.create<VarDeclaration>(span, visibility, storage_kind, std::move(name),
                                                    type_expression, initializer);
    }

    ReturnStatement* parse_return_statement() {
        auto span = current_token.span;

        expect(Token::Type::Return);

        auto* value = static_cast<Expression*>(nullptr);
        if (!check(Token::Type::Eof) && !check(Token::Type::Semicolon)) {
            value = parse_expression();
        }

        return context.nodes.create<ReturnStatement>(span, value);
    }

    BlockStatement* parse_block_statement() {
        auto span = current_token.span;

        expect(Token::Type::LeftBrace);

        auto statements = std::vector<Statement*>{};
        while (!check(Token::Type::RightBrace) && !check(Token::Type::Eof)) {
            statements.push_back(parse_statement());

            if (check(Token::Type::Semicolon)) {
                advance();
            }
        }

        expect(Token::Type::RightBrace);

        return context.nodes.create<BlockStatement>(span, std::move(statements));
    }

    Statement* parse_extern_declaration(Visibility::Type visibility) {
        auto span = current_token.span;

        expect(Token::Type::Extern);

        switch (current_token.type) {
        case Token::Type::Fn: {
            advance();

            auto* prototype = parse_function_prototype();
            return context.nodes.create<ExternFunctionDeclaration>(span, visibility, prototype);
        }
        case Token::Type::Var: {
            advance();

            auto name = std::string(expect(Token::Type::Identifier).value);
            expect(Token::Type::Colon);
            auto* type_expression = parse_type_expression();

            return context.nodes.create<ExternVarDeclaration>(span, visibility, std::move(name),
                                                              type_expression);
        }
        default:
            logger.error(span, "expected 'fn' or 'var' after 'extern'");
            break;
        }
    }

  public:
    Parser(Context& context, Lexer lexer)
        : context(context), lexer(std::move(lexer)), logger(context.logger),
          current_token(this->lexer.next_token()), peek_token(this->lexer.next_token()) {}

    Program parse() {
        Program program;

        while (!check(Token::Type::Eof)) {
            program.statements.push_back(parse_statement());

            if (check(Token::Type::Semicolon)) {
                advance();
            }
        }

        return program;
    }
};
