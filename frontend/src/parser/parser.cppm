module;

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.parser;

import zep.frontend.ast;
import zep.frontend.ast.program;
import zep.frontend.token;
import zep.frontend.lexer;
import zep.frontend.parser.precedence;
import zep.common.logger;
import zep.frontend.sema.type;
import zep.frontend.sema.kinds;

export class Parser {
  private:
    Lexer lexer;

    const Logger& logger;

    Token current_token;
    Token peek_token;

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

    std::unique_ptr<Statement> parse_statement() {
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
        default:
            return parse_expression_statement();
        }
    }

    std::unique_ptr<ExpressionStatement> parse_expression_statement() {
        auto expression = parse_expression();
        return std::make_unique<ExpressionStatement>(std::move(expression));
    }

    std::unique_ptr<Expression>
    parse_expression(Precedence::Type precedence = Precedence::Type::None) {
        auto left = parse_prefix();
        while (get_precedence(current_token.type) > precedence) {
            left = parse_infix(std::move(left));
        }

        return left;
    }

    std::unique_ptr<Expression> parse_prefix() {
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

    std::unique_ptr<Expression> parse_unary_expression() {
        auto span = current_token.span;

        UnaryExpression::Operator::Type op;

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

        auto operand = parse_expression(Precedence::Type::Unary);
        return std::make_unique<UnaryExpression>(span, op, std::move(operand));
    }

    std::unique_ptr<Expression> parse_primary() {
        auto span = current_token.span;

        switch (current_token.type) {

        case Token::Type::String: {
            auto value = std::string(current_token.value);
            advance();

            return std::make_unique<StringLiteral>(span, std::move(value));
        }
        case Token::Type::Number: {
            auto value = std::string(current_token.value);
            advance();

            return std::make_unique<NumberLiteral>(span, std::move(value));
        }
        case Token::Type::Float: {
            auto value = std::string(current_token.value);
            advance();

            return std::make_unique<FloatLiteral>(span, std::move(value));
        }
        case Token::Type::Boolean: {
            auto value = current_token.value == "true";
            advance();

            return std::make_unique<BooleanLiteral>(span, value);
        }
        case Token::Type::Identifier: {
            auto name = std::string(current_token.value);
            advance();

            if (check(Token::Type::LessThan)) {
                auto generic_arguments = parse_generic_arguments();

                if (check(Token::Type::LeftParen)) {
                    auto callee = std::make_unique<IdentifierExpression>(span, std::move(name));
                    return parse_call_expression(std::move(callee), std::move(generic_arguments));
                }

                if (check(Token::Type::LeftBrace)) {
                    auto type_name =
                        std::make_unique<IdentifierExpression>(span, std::move(name));
                    return parse_struct_literal(std::move(type_name), std::move(generic_arguments));
                }

                logger.error(current_token.span, "expected '(' or '{' after generic arguments");
            }

            if (check(Token::Type::LeftBrace)) {
                auto type_name = std::make_unique<IdentifierExpression>(span, std::move(name));
                return parse_struct_literal(std::move(type_name), {});
            }

            return std::make_unique<IdentifierExpression>(span, std::move(name));
        }
        case Token::Type::LeftParen: {
            advance();

            auto expression = parse_expression();
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

    std::unique_ptr<Expression> parse_infix(std::unique_ptr<Expression> left) {
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
            return parse_binary_expression(std::move(left));
        case Token::Type::Assign: {
            auto span = current_token.span;

            advance();

            auto right = parse_expression(
                static_cast<Precedence::Type>(static_cast<int>(Precedence::Type::Assignment) - 1));
            return std::make_unique<AssignExpression>(span, std::move(left), std::move(right));
        }
        case Token::Type::LeftParen:
        case Token::Type::LeftBracket:
        case Token::Type::Dot:
            return parse_postfix(std::move(left));
        default:
            logger.error(current_token.span,
                         "unexpected token '" + std::string(current_token.value) + "'");
        }
    }

    std::unique_ptr<Expression> parse_postfix(std::unique_ptr<Expression> left) {
        switch (current_token.type) {
        case Token::Type::LeftParen:
            return parse_call_expression(std::move(left));
        case Token::Type::LeftBracket:
            return parse_index_expression(std::move(left));
        case Token::Type::Dot:
            return parse_member_expression(std::move(left));
        default:
            logger.error(current_token.span,
                         "unexpected token '" + std::string(current_token.value) + "'");
        }
    }

    std::unique_ptr<Expression> parse_binary_expression(std::unique_ptr<Expression> left) {
        auto span = current_token.span;

        BinaryExpression::Operator::Type op;

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

        auto precedence = get_precedence(current_token.type);

        advance();

        if (op == BinaryExpression::Operator::Type::As || op == BinaryExpression::Operator::Type::Is) {
            auto type_expression = parse_type_expression();
            auto right = std::make_unique<IdentifierExpression>(
                span, type_expression->get_type()->to_string());
            right->set_type(type_expression->get_type());
            return std::make_unique<BinaryExpression>(span, std::move(left), op,
                                                      std::move(right));
        }

        auto right = parse_expression(precedence);
        return std::make_unique<BinaryExpression>(span, std::move(left), op, std::move(right));
    }

    std::vector<std::unique_ptr<Parameter>> parse_parameters() {
        std::vector<std::unique_ptr<Parameter>> parameters;

        while (!check(Token::Type::RightParen) && !check(Token::Type::Eof)) {
            auto span = current_token.span;

            auto is_variadic = false;
            if (match(Token::Type::Ellipsis)) {
                is_variadic = true;
            }

            auto name = std::string(expect(Token::Type::Identifier).value);
            expect(Token::Type::Colon);
            auto type = parse_type_expression();

            parameters.push_back(std::make_unique<Parameter>(span, is_variadic, std::move(name),
                                                             std::move(type)));

            if (!check(Token::Type::RightParen)) {
                expect(Token::Type::Comma);
            }
        }

        return parameters;
    }

    std::vector<std::unique_ptr<Argument>> parse_arguments() {
        std::vector<std::unique_ptr<Argument>> arguments;

        while (!check(Token::Type::RightParen) && !check(Token::Type::Eof)) {
            auto span = current_token.span;
            auto value = parse_expression();

            arguments.push_back(std::make_unique<Argument>(span, "", std::move(value)));

            if (!check(Token::Type::RightParen)) {
                expect(Token::Type::Comma);
            }
        }

        return arguments;
    }

    std::vector<std::unique_ptr<GenericParameter>> parse_generic_parameters() {
        std::vector<std::unique_ptr<GenericParameter>> generic_parameters;

        expect(Token::Type::LessThan);
        while (!check(Token::Type::GreaterThan) && !check(Token::Type::Eof)) {
            auto span = current_token.span;

            auto name = std::string(expect(Token::Type::Identifier).value);

            std::unique_ptr<TypeExpression> constraint;
            if (match(Token::Type::Colon)) {
                constraint = parse_type_expression();
            }

            generic_parameters.push_back(std::make_unique<GenericParameter>(
                span, std::move(name), std::move(constraint)));

            if (!check(Token::Type::GreaterThan)) {
                expect(Token::Type::Comma);
            }
        }
        expect(Token::Type::GreaterThan);

        return generic_parameters;
    }

    std::vector<std::unique_ptr<GenericArgument>> parse_generic_arguments() {
        std::vector<std::unique_ptr<GenericArgument>> generic_arguments;

        expect(Token::Type::LessThan);
        while (!check(Token::Type::GreaterThan) && !check(Token::Type::Eof)) {
            auto span = current_token.span;

            auto type = parse_type_expression();

            generic_arguments.push_back(
                std::make_unique<GenericArgument>(span, "", std::move(type)));

            if (!check(Token::Type::GreaterThan)) {
                expect(Token::Type::Comma);
            }
        }
        expect(Token::Type::GreaterThan);

        return generic_arguments;
    }

    std::shared_ptr<Type> resolve_type(const std::string& name) {
        if (name == "void") {
            return std::make_shared<VoidType>();
        }

        if (name == "string") {
            return std::make_shared<StringType>();
        }

        if (name == "boolean") {
            return std::make_shared<BooleanType>();
        }

        if (name.size() >= 2 && (name[0] == 'i' || name[0] == 'u')) {
            auto is_unsigned = name[0] == 'u';
            try {
                auto size = std::stoi(name.substr(1));
                return std::make_shared<IntegerType>(is_unsigned, static_cast<std::uint16_t>(size));
            } catch (...) {}
        }

        if (name.size() >= 2 && name[0] == 'f') {
            try {
                auto size = std::stoi(name.substr(1));
                return std::make_shared<FloatType>(static_cast<std::uint16_t>(size));
            } catch (...) {}
        }

        return std::make_shared<NamedType>(name);
    }

    std::unique_ptr<TypeExpression> parse_type_expression() {
        auto span = current_token.span;

        if (check(Token::Type::Asterisk)) {
            advance();

            bool is_mutable = false;
            if (check(Token::Type::Mut)) {
                is_mutable = true;
                advance();
            }

            auto element = parse_type_expression();

            auto type = std::make_shared<PointerType>(element->get_type(), is_mutable);
            return std::make_unique<TypeExpression>(span, std::move(type));
        }

        auto name = std::string(expect(Token::Type::Identifier).value);
        auto type = resolve_type(name);

        if (check(Token::Type::LessThan) && type->kind == Type::Kind::Type::Named) {
            auto generic_arguments = parse_generic_arguments();

            std::vector<std::shared_ptr<GenericArgumentType>> generic_argument_types;
            generic_argument_types.reserve(generic_arguments.size());

            for (auto& generic_argument : generic_arguments) {
                generic_argument_types.emplace_back(
                    std::make_shared<GenericArgumentType>(generic_argument->type->get_type()));
            }

            type = std::make_shared<NamedType>(name, std::move(generic_argument_types));
        }

        while (check(Token::Type::LeftBracket)) {
            advance();

            std::optional<std::size_t> size;
            if (check(Token::Type::Number)) {
                size = std::stoull(std::string(current_token.value));
                advance();
            }

            expect(Token::Type::RightBracket);

            type = std::make_shared<ArrayType>(std::move(type), size);
        }

        return std::make_unique<TypeExpression>(span, std::move(type));
    }

    std::unique_ptr<IfExpression> parse_if_expression() {
        auto span = current_token.span;

        expect(Token::Type::If);

        expect(Token::Type::LeftParen);
        auto condition = parse_expression();
        expect(Token::Type::RightParen);

        auto then_branch = parse_block_statement();

        std::unique_ptr<Statement> else_branch;
        if (match(Token::Type::Else)) {
            else_branch = parse_block_statement();
        }

        return std::make_unique<IfExpression>(span, std::move(condition),
                                              std::move(then_branch), std::move(else_branch));
    }

    std::unique_ptr<StructLiteralExpression>
    parse_struct_literal(std::unique_ptr<IdentifierExpression> name,
                         std::vector<std::unique_ptr<GenericArgument>> generic_arguments = {}) {
        auto span = name->span;

        expect(Token::Type::LeftBrace);

        std::vector<std::unique_ptr<StructLiteralField>> fields;
        while (!check(Token::Type::RightBrace) && !check(Token::Type::Eof)) {
            auto span = current_token.span;

            auto name = std::string(expect(Token::Type::Identifier).value);
            expect(Token::Type::Colon);
            auto value = parse_expression();

            fields.push_back(
                std::make_unique<StructLiteralField>(span, std::move(name), std::move(value)));

            if (!check(Token::Type::RightBrace)) {
                expect(Token::Type::Comma);
            }
        }

        expect(Token::Type::RightBrace);

        return std::make_unique<StructLiteralExpression>(
            span, std::move(name), std::move(generic_arguments), std::move(fields));
    }

    std::unique_ptr<CallExpression>
    parse_call_expression(std::unique_ptr<Expression> callee,
                          std::vector<std::unique_ptr<GenericArgument>> generic_arguments = {}) {
        auto span = current_token.span;

        expect(Token::Type::LeftParen);
        auto arguments = parse_arguments();
        expect(Token::Type::RightParen);

        return std::make_unique<CallExpression>(span, std::move(callee),
                                                std::move(generic_arguments), std::move(arguments));
    }

    std::unique_ptr<IndexExpression> parse_index_expression(std::unique_ptr<Expression> value) {
        auto span = current_token.span;

        expect(Token::Type::LeftBracket);
        auto index = parse_expression();
        expect(Token::Type::RightBracket);

        return std::make_unique<IndexExpression>(span, std::move(value), std::move(index));
    }

    std::unique_ptr<MemberExpression> parse_member_expression(std::unique_ptr<Expression> value) {
        auto span = current_token.span;

        expect(Token::Type::Dot);
        auto member = std::string(expect(Token::Type::Identifier).value);

        return std::make_unique<MemberExpression>(span, std::move(value), std::move(member));
    }

    std::unique_ptr<ImportStatement> parse_import_statement() {
        auto span = current_token.span;

        expect(Token::Type::Import);

        std::vector<std::unique_ptr<IdentifierExpression>> path;

        auto first = expect(Token::Type::Identifier);
        path.push_back(
            std::make_unique<IdentifierExpression>(first.span, std::string(first.value)));

        while (match(Token::Type::Dot)) {
            auto part = expect(Token::Type::Identifier);
            path.push_back(
                std::make_unique<IdentifierExpression>(part.span, std::string(part.value)));
        }

        return std::make_unique<ImportStatement>(span, std::move(path));
    }

    std::unique_ptr<StructDeclaration> parse_struct_declaration(Visibility::Type visibility) {
        auto span = current_token.span;

        expect(Token::Type::Struct);

        auto name = std::string(expect(Token::Type::Identifier).value);

        std::vector<std::unique_ptr<GenericParameter>> generic_parameters;
        if (check(Token::Type::LessThan)) {
            generic_parameters = parse_generic_parameters();
        }

        expect(Token::Type::LeftBrace);

        std::vector<std::unique_ptr<StructField>> fields;
        while (!check(Token::Type::RightBrace) && !check(Token::Type::Eof)) {
            auto span = current_token.span;

            auto field_visibility = Visibility::Type::Public;
            if (check(Token::Type::Public) || check(Token::Type::Private)) {
                field_visibility = parse_visibility();
            }

            auto name = std::string(expect(Token::Type::Identifier).value);
            expect(Token::Type::Colon);
            auto type = parse_type_expression();

            fields.push_back(std::make_unique<StructField>(span, field_visibility,
                                                           std::move(name), std::move(type)));
        }

        expect(Token::Type::RightBrace);

        return std::make_unique<StructDeclaration>(span, visibility, std::move(name),
                                                   std::move(generic_parameters),
                                                   std::move(fields));
    }

    std::unique_ptr<FunctionPrototype> parse_function_prototype() {
        auto span = current_token.span;

        auto name = std::string(expect(Token::Type::Identifier).value);

        std::vector<std::unique_ptr<GenericParameter>> generic_parameters;
        if (check(Token::Type::LessThan)) {
            generic_parameters = parse_generic_parameters();
        }

        expect(Token::Type::LeftParen);
        auto parameters = parse_parameters();
        expect(Token::Type::RightParen);

        expect(Token::Type::Colon);
        auto return_type = parse_type_expression();

        return std::make_unique<FunctionPrototype>(span, std::move(name),
                                                   std::move(generic_parameters),
                                                   std::move(parameters), std::move(return_type));
    }

    std::unique_ptr<FunctionDeclaration> parse_function_declaration(Visibility::Type visibility) {
        auto span = current_token.span;

        expect(Token::Type::Fn);

        auto prototype = parse_function_prototype();
        auto body = parse_block_statement();

        return std::make_unique<FunctionDeclaration>(span, visibility, std::move(prototype),
                                                     std::move(body));
    }

    std::unique_ptr<VarDeclaration> parse_var_declaration(Visibility::Type visibility) {
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

        std::unique_ptr<TypeExpression> type;
        if (match(Token::Type::Colon)) {
            type = parse_type_expression();
        }

        std::unique_ptr<Expression> initializer;
        if (match(Token::Type::Assign)) {
            initializer = parse_expression();
        }

        return std::make_unique<VarDeclaration>(span, visibility, storage_kind, std::move(name),
                                                std::move(type), std::move(initializer));
    }

    std::unique_ptr<ReturnStatement> parse_return_statement() {
        auto span = current_token.span;

        expect(Token::Type::Return);

        std::unique_ptr<Expression> value;
        if (!check(Token::Type::Eof) && !check(Token::Type::Semicolon)) {
            value = parse_expression();
        }

        return std::make_unique<ReturnStatement>(span, std::move(value));
    }

    std::unique_ptr<BlockStatement> parse_block_statement() {
        auto span = current_token.span;

        expect(Token::Type::LeftBrace);

        std::vector<std::unique_ptr<Statement>> statements;
        while (!check(Token::Type::RightBrace) && !check(Token::Type::Eof)) {
            statements.push_back(parse_statement());

            if (check(Token::Type::Semicolon)) {
                advance();
            }
        }

        expect(Token::Type::RightBrace);

        return std::make_unique<BlockStatement>(span, std::move(statements));
    }

    std::unique_ptr<Statement> parse_extern_declaration(Visibility::Type visibility) {
        auto span = current_token.span;

        expect(Token::Type::Extern);

        switch (current_token.type) {
        case Token::Type::Fn: {
            advance();

            auto prototype = parse_function_prototype();
            return std::make_unique<ExternFunctionDeclaration>(span, visibility,
                                                               std::move(prototype));
        }
        case Token::Type::Var: {
            advance();

            auto name = std::string(expect(Token::Type::Identifier).value);
            expect(Token::Type::Colon);
            auto type = parse_type_expression();

            return std::make_unique<ExternVarDeclaration>(span, visibility, std::move(name),
                                                          std::move(type));
        }
        default:
            logger.error(span, "expected 'fn' or 'var' after 'extern'");
            break;
        }
    }

  public:
    Parser(Lexer lexer, const Logger& logger)
        : lexer(std::move(lexer)), logger(logger), current_token(this->lexer.next_token()),
          peek_token(this->lexer.next_token()) {}

    Program parse() {
        std::vector<std::unique_ptr<Statement>> statements;
        while (!check(Token::Type::Eof)) {
            statements.push_back(parse_statement());

            if (check(Token::Type::Semicolon)) {
                advance();
            }
        }

        return Program(std::move(statements));
    }
};
