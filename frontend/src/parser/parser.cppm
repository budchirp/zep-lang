module;

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

export module zep.frontend.parser;

import zep.frontend.ast;
import zep.frontend.ast.program;
import zep.frontend.token;
import zep.frontend.token.type;
import zep.frontend.lexer;
import zep.frontend.parser.precedence;
import zep.common.position;
import zep.common.logger;
import zep.sema.type;

export class Parser {
  private:
    Lexer lexer;
    Logger logger;

    Token current_token;
    Token peek_token;

    bool check(TokenType type) const { return current_token.type == type; }

    bool peek(TokenType type) const { return peek_token.type == type; }

    void advance() {
        current_token = peek_token;
        peek_token = lexer.next_token();
    }

    bool match(TokenType type) {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }

    Token expect(TokenType type) {
        if (!check(type)) {
            logger.error(current_token.position,
                         "unexpected token '" + std::string(current_token.value) + "'");
        }
        auto token = current_token;
        advance();
        return token;
    }

    Visibility parse_visibility() {
        if (match(TokenType::Public)) {
            return Visibility::Public;
        }
        if (match(TokenType::Private)) {
            return Visibility::Private;
        }
        return Visibility::Public;
    }

    std::shared_ptr<Type> resolve_type(const std::string& name) {
        if (name == "void") {
            return std::make_shared<VoidType>();
        }
        if (name == "string") {
            return std::make_shared<StringType>();
        }
        if (name == "bool") {
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

    std::vector<std::unique_ptr<GenericParameter>> parse_generic_parameters() {
        std::vector<std::unique_ptr<GenericParameter>> generic_parameters;

        expect(TokenType::LessThan);
        while (!check(TokenType::GreaterThan) && !check(TokenType::Eof)) {
            auto position = current_token.position;
            auto name = std::string(expect(TokenType::Identifier).value);

            std::unique_ptr<TypeExpression> constraint;
            if (match(TokenType::Colon)) {
                constraint = parse_type_expression();
            }

            generic_parameters.push_back(std::make_unique<GenericParameter>(
                position, std::move(name), std::move(constraint)));

            if (!check(TokenType::GreaterThan)) {
                expect(TokenType::Comma);
            }
        }
        expect(TokenType::GreaterThan);

        return generic_parameters;
    }

    std::vector<std::unique_ptr<GenericArgument>> parse_generic_arguments() {
        std::vector<std::unique_ptr<GenericArgument>> generic_arguments;

        expect(TokenType::LessThan);
        while (!check(TokenType::GreaterThan) && !check(TokenType::Eof)) {
            auto position = current_token.position;
            auto type = parse_type_expression();

            generic_arguments.push_back(
                std::make_unique<GenericArgument>(position, "", std::move(type)));

            if (!check(TokenType::GreaterThan)) {
                expect(TokenType::Comma);
            }
        }
        expect(TokenType::GreaterThan);

        return generic_arguments;
    }

    std::unique_ptr<TypeExpression> parse_type_expression() {
        auto position = current_token.position;

        if (check(TokenType::Asterisk)) {
            advance();
            auto inner = parse_type_expression();
            auto pointer_type = std::make_shared<PointerType>(inner->get_type());
            return std::make_unique<TypeExpression>(position, std::move(pointer_type));
        }

        auto name_token = expect(TokenType::Identifier);
        auto name = std::string(name_token.value);
        auto type = resolve_type(name);

        if (check(TokenType::LessThan) && type->kind == TypeKind::Named) {
            auto generic_args = parse_generic_arguments();

            std::vector<GenericArgumentType> ga_types;
            for (auto& arg : generic_args) {
                ga_types.push_back(GenericArgumentType(arg->type->get_type()));
            }
            type = std::make_shared<NamedType>(name, std::move(ga_types));
        }

        while (check(TokenType::LeftBracket)) {
            advance();

            std::optional<std::size_t> size;
            if (check(TokenType::Number)) {
                size = std::stoull(std::string(current_token.value));
                advance();
            }

            expect(TokenType::RightBracket);
            type = std::make_shared<ArrayType>(std::move(type), size);
        }

        return std::make_unique<TypeExpression>(position, std::move(type));
    }

    std::vector<std::unique_ptr<Parameter>> parse_parameters() {
        std::vector<std::unique_ptr<Parameter>> parameters;

        while (!check(TokenType::RightParen) && !check(TokenType::Eof)) {
            auto position = current_token.position;

            auto is_variadic = false;
            if (match(TokenType::Ellipsis)) {
                is_variadic = true;
            }

            auto name = std::string(expect(TokenType::Identifier).value);
            expect(TokenType::Colon);
            auto type = parse_type_expression();

            parameters.push_back(std::make_unique<Parameter>(position, is_variadic, std::move(name),
                                                             std::move(type)));

            if (!check(TokenType::RightParen)) {
                expect(TokenType::Comma);
            }
        }

        return parameters;
    }

    std::vector<std::unique_ptr<Argument>> parse_arguments() {
        std::vector<std::unique_ptr<Argument>> arguments;

        while (!check(TokenType::RightParen) && !check(TokenType::Eof)) {
            auto position = current_token.position;
            auto value = parse_expression();

            arguments.push_back(std::make_unique<Argument>(position, "", std::move(value)));

            if (!check(TokenType::RightParen)) {
                expect(TokenType::Comma);
            }
        }

        return arguments;
    }

    std::vector<std::unique_ptr<StructField>> parse_struct_fields() {
        std::vector<std::unique_ptr<StructField>> fields;

        while (!check(TokenType::RightBrace) && !check(TokenType::Eof)) {
            auto position = current_token.position;

            auto field_visibility = Visibility::Public;
            if (check(TokenType::Public) || check(TokenType::Private)) {
                field_visibility = parse_visibility();
            }

            auto name = std::string(expect(TokenType::Identifier).value);
            expect(TokenType::Colon);
            auto type = parse_type_expression();

            fields.push_back(std::make_unique<StructField>(position, field_visibility,
                                                           std::move(name), std::move(type)));
        }

        return fields;
    }

    std::vector<std::unique_ptr<StructLiteralField>> parse_struct_literal_fields() {
        std::vector<std::unique_ptr<StructLiteralField>> fields;

        while (!check(TokenType::RightBrace) && !check(TokenType::Eof)) {
            auto position = current_token.position;
            auto name = std::string(expect(TokenType::Identifier).value);
            expect(TokenType::Colon);
            auto value = parse_expression();

            fields.push_back(
                std::make_unique<StructLiteralField>(position, std::move(name), std::move(value)));

            if (!check(TokenType::RightBrace)) {
                expect(TokenType::Comma);
            }
        }

        return fields;
    }

    std::unique_ptr<FunctionPrototype> parse_function_prototype() {
        auto position = current_token.position;
        auto name = std::string(expect(TokenType::Identifier).value);

        std::vector<std::unique_ptr<GenericParameter>> generic_parameters;
        if (check(TokenType::LessThan)) {
            generic_parameters = parse_generic_parameters();
        }

        expect(TokenType::LeftParen);
        auto parameters = parse_parameters();
        expect(TokenType::RightParen);

        expect(TokenType::Colon);
        auto return_type = parse_type_expression();

        return std::make_unique<FunctionPrototype>(position, std::move(name),
                                                   std::move(generic_parameters),
                                                   std::move(parameters), std::move(return_type));
    }

    std::unique_ptr<Expression> parse_prefix() {
        auto position = current_token.position;

        switch (current_token.type) {
        case TokenType::Number: {
            auto value = std::string(current_token.value);
            advance();
            return std::make_unique<NumberLiteral>(position, std::move(value));
        }
        case TokenType::Float: {
            auto value = std::string(current_token.value);
            advance();
            return std::make_unique<FloatLiteral>(position, std::move(value));
        }
        case TokenType::String: {
            auto value = std::string(current_token.value);
            advance();
            return std::make_unique<StringLiteral>(position, std::move(value));
        }
        case TokenType::Boolean: {
            auto value = current_token.value == "true";
            advance();
            return std::make_unique<BooleanLiteral>(position, value);
        }
        case TokenType::Identifier: {
            auto name = std::string(current_token.value);
            advance();

            if (check(TokenType::LessThan) &&
                (peek(TokenType::Identifier) || peek(TokenType::Asterisk))) {
                auto generic_args = parse_generic_arguments();

                if (check(TokenType::LeftParen)) {
                    auto callee = std::make_unique<IdentifierExpression>(position, std::move(name));
                    return parse_call_expression(std::move(callee), std::move(generic_args));
                }

                if (check(TokenType::LeftBrace)) {
                    std::vector<GenericArgumentType> ga_types;
                    for (auto& arg : generic_args) {
                        ga_types.push_back(GenericArgumentType(arg->type->get_type()));
                    }
                    auto type = std::make_shared<NamedType>(name, std::move(ga_types));
                    auto type_expression =
                        std::make_unique<TypeExpression>(position, std::move(type));
                    return parse_struct_literal(std::move(type_expression));
                }

                logger.error(current_token.position, "expected '(' or '{' after generic arguments");
            }

            if (check(TokenType::LeftBrace)) {
                auto type = std::make_shared<NamedType>(name);
                auto type_expression = std::make_unique<TypeExpression>(position, std::move(type));
                return parse_struct_literal(std::move(type_expression));
            }

            return std::make_unique<IdentifierExpression>(position, std::move(name));
        }
        case TokenType::Minus: {
            advance();
            auto operand = parse_expression(Precedence::Unary);
            return std::make_unique<UnaryExpression>(
                position, UnaryExpression::UnaryOperator::Minus, std::move(operand));
        }
        case TokenType::Plus: {
            advance();
            auto operand = parse_expression(Precedence::Unary);
            return std::make_unique<UnaryExpression>(position, UnaryExpression::UnaryOperator::Plus,
                                                     std::move(operand));
        }
        case TokenType::Not: {
            advance();
            auto operand = parse_expression(Precedence::Unary);
            return std::make_unique<UnaryExpression>(position, UnaryExpression::UnaryOperator::Not,
                                                     std::move(operand));
        }
        case TokenType::Asterisk: {
            advance();
            auto operand = parse_expression(Precedence::Unary);
            return std::make_unique<UnaryExpression>(
                position, UnaryExpression::UnaryOperator::Dereference, std::move(operand));
        }
        case TokenType::Ampersand: {
            advance();
            auto operand = parse_expression(Precedence::Unary);
            return std::make_unique<UnaryExpression>(
                position, UnaryExpression::UnaryOperator::AddressOf, std::move(operand));
        }
        case TokenType::LeftParen: {
            advance();
            auto expression = parse_expression();
            expect(TokenType::RightParen);
            return expression;
        }
        default:
            logger.error(position, "unexpected token '" + std::string(current_token.value) +
                                       "' in expression");
        }
    }

    std::unique_ptr<CallExpression>
    parse_call_expression(std::unique_ptr<Expression> callee,
                          std::vector<std::unique_ptr<GenericArgument>> generic_arguments = {}) {
        auto position = current_token.position;
        expect(TokenType::LeftParen);
        auto arguments = parse_arguments();
        expect(TokenType::RightParen);
        return std::make_unique<CallExpression>(position, std::move(callee),
                                                std::move(generic_arguments), std::move(arguments));
    }

    std::unique_ptr<IndexExpression> parse_index_expression(std::unique_ptr<Expression> value) {
        auto position = current_token.position;
        expect(TokenType::LeftBracket);
        auto index = parse_expression();
        expect(TokenType::RightBracket);
        return std::make_unique<IndexExpression>(position, std::move(value), std::move(index));
    }

    std::unique_ptr<MemberExpression> parse_member_expression(std::unique_ptr<Expression> value) {
        auto position = current_token.position;
        expect(TokenType::Dot);
        auto member = std::string(expect(TokenType::Identifier).value);
        return std::make_unique<MemberExpression>(position, std::move(value), std::move(member));
    }

    std::unique_ptr<Expression> parse_postfix(std::unique_ptr<Expression> left) {
        if (check(TokenType::LeftParen)) {
            return parse_call_expression(std::move(left));
        }
        if (check(TokenType::LeftBracket)) {
            return parse_index_expression(std::move(left));
        }
        if (check(TokenType::Dot)) {
            return parse_member_expression(std::move(left));
        }

        logger.error(current_token.position,
                     "unexpected token '" + std::string(current_token.value) + "'");
    }

    std::unique_ptr<Expression> parse_binary(std::unique_ptr<Expression> left) {
        auto position = current_token.position;
        auto token_type = current_token.type;
        auto precedence = get_precedence(token_type);

        BinaryExpression::BinaryOperator op;
        switch (token_type) {
        case TokenType::Plus:
            op = BinaryExpression::BinaryOperator::Plus;
            break;
        case TokenType::Minus:
            op = BinaryExpression::BinaryOperator::Minus;
            break;
        case TokenType::Asterisk:
            op = BinaryExpression::BinaryOperator::Asterisk;
            break;
        case TokenType::Divide:
            op = BinaryExpression::BinaryOperator::Divide;
            break;
        case TokenType::Modulo:
            op = BinaryExpression::BinaryOperator::Modulo;
            break;
        case TokenType::Equals:
            op = BinaryExpression::BinaryOperator::Equals;
            break;
        case TokenType::NotEquals:
            op = BinaryExpression::BinaryOperator::NotEquals;
            break;
        case TokenType::LessThan:
            op = BinaryExpression::BinaryOperator::LessThan;
            break;
        case TokenType::GreaterThan:
            op = BinaryExpression::BinaryOperator::GreaterThan;
            break;
        case TokenType::LessEqual:
            op = BinaryExpression::BinaryOperator::LessEqual;
            break;
        case TokenType::GreaterEqual:
            op = BinaryExpression::BinaryOperator::GreaterEqual;
            break;
        case TokenType::And:
            op = BinaryExpression::BinaryOperator::And;
            break;
        case TokenType::Or:
            op = BinaryExpression::BinaryOperator::Or;
            break;
        case TokenType::As:
            op = BinaryExpression::BinaryOperator::As;
            break;
        case TokenType::Is:
            op = BinaryExpression::BinaryOperator::Is;
            break;
        default:
            logger.error(position, "unexpected token '" + std::string(current_token.value) +
                                       "' in binary expression");
        }

        advance();
        auto right = parse_expression(precedence);
        return std::make_unique<BinaryExpression>(position, std::move(left), op, std::move(right));
    }

    std::unique_ptr<Expression> parse_infix(std::unique_ptr<Expression> left) {
        auto token_type = current_token.type;

        switch (token_type) {
        case TokenType::Plus:
        case TokenType::Minus:
        case TokenType::Asterisk:
        case TokenType::Divide:
        case TokenType::Modulo:
        case TokenType::Equals:
        case TokenType::NotEquals:
        case TokenType::LessThan:
        case TokenType::GreaterThan:
        case TokenType::LessEqual:
        case TokenType::GreaterEqual:
        case TokenType::And:
        case TokenType::Or:
        case TokenType::As:
        case TokenType::Is:
            return parse_binary(std::move(left));
        case TokenType::Assign: {
            auto position = current_token.position;
            advance();
            auto right = parse_expression(
                static_cast<Precedence>(static_cast<int>(Precedence::Assignment) - 1));
            return std::make_unique<AssignExpression>(position, std::move(left), std::move(right));
        }
        case TokenType::LeftParen:
        case TokenType::LeftBracket:
        case TokenType::Dot:
            return parse_postfix(std::move(left));
        default:
            logger.error(current_token.position,
                         "unexpected token '" + std::string(current_token.value) + "'");
        }
    }

    std::unique_ptr<Expression> parse_expression(Precedence precedence = Precedence::None) {
        auto left = parse_prefix();

        while (get_precedence(current_token.type) > precedence) {
            left = parse_infix(std::move(left));
        }

        return left;
    }

    std::unique_ptr<StructLiteralExpression>
    parse_struct_literal(std::unique_ptr<TypeExpression> type_name) {
        auto position = type_name->get_position();

        expect(TokenType::LeftBrace);
        auto fields = parse_struct_literal_fields();
        expect(TokenType::RightBrace);

        return std::make_unique<StructLiteralExpression>(position, std::move(type_name),
                                                         std::move(fields));
    }

    std::unique_ptr<BlockStatement> parse_block_statement() {
        auto position = current_token.position;
        expect(TokenType::LeftBrace);

        std::vector<std::unique_ptr<Statement>> statements;
        while (!check(TokenType::RightBrace) && !check(TokenType::Eof)) {
            statements.push_back(parse_statement());
        }

        expect(TokenType::RightBrace);
        return std::make_unique<BlockStatement>(position, std::move(statements));
    }

    std::unique_ptr<ExpressionStatement> parse_expression_statement() {
        auto expression = parse_expression();
        match(TokenType::Semicolon);
        return std::make_unique<ExpressionStatement>(std::move(expression));
    }

    std::unique_ptr<ReturnStatement> parse_return_statement() {
        auto position = current_token.position;
        advance();

        std::unique_ptr<Expression> value;
        if (!check(TokenType::RightBrace) && !check(TokenType::Eof) &&
            !check(TokenType::Semicolon)) {
            value = parse_expression();
        }

        match(TokenType::Semicolon);
        return std::make_unique<ReturnStatement>(position, std::move(value));
    }

    std::unique_ptr<IfStatement> parse_if_statement() {
        auto position = current_token.position;
        advance();

        expect(TokenType::LeftParen);
        auto condition = parse_expression();
        expect(TokenType::RightParen);

        auto then_branch = parse_block_statement();

        std::unique_ptr<Statement> else_branch;
        if (match(TokenType::Else)) {
            if (check(TokenType::If)) {
                else_branch = parse_if_statement();
            } else {
                else_branch = parse_block_statement();
            }
        }

        return std::make_unique<IfStatement>(position, std::move(condition), std::move(then_branch),
                                             std::move(else_branch));
    }

    std::unique_ptr<VarDeclaration> parse_var_declaration(Visibility visibility) {
        auto position = current_token.position;

        auto storage_kind = StorageKind::Var;
        if (check(TokenType::Const)) {
            storage_kind = StorageKind::Const;
        }
        advance();

        if (storage_kind == StorageKind::Var && match(TokenType::Mut)) {
            storage_kind = StorageKind::VarMut;
        }

        auto name = std::string(expect(TokenType::Identifier).value);

        std::unique_ptr<TypeExpression> type;
        if (match(TokenType::Colon)) {
            type = parse_type_expression();
        }

        std::unique_ptr<Expression> initializer;
        if (match(TokenType::Assign)) {
            initializer = parse_expression();
        }

        match(TokenType::Semicolon);
        return std::make_unique<VarDeclaration>(position, visibility, storage_kind, std::move(name),
                                                std::move(type), std::move(initializer));
    }

    std::unique_ptr<FunctionDeclaration> parse_function_declaration(Visibility visibility) {
        auto position = current_token.position;
        advance();

        auto prototype = parse_function_prototype();
        auto body = parse_block_statement();

        return std::make_unique<FunctionDeclaration>(position, visibility, std::move(prototype),
                                                     std::move(body));
    }

    std::unique_ptr<StructDeclaration> parse_struct_declaration(Visibility visibility) {
        auto position = current_token.position;
        advance();

        auto name = std::string(expect(TokenType::Identifier).value);

        std::vector<std::unique_ptr<GenericParameter>> generic_parameters;
        if (check(TokenType::LessThan)) {
            generic_parameters = parse_generic_parameters();
        }

        expect(TokenType::LeftBrace);
        auto fields = parse_struct_fields();
        expect(TokenType::RightBrace);

        return std::make_unique<StructDeclaration>(position, visibility, std::move(name),
                                                   std::move(generic_parameters),
                                                   std::move(fields));
    }

    std::unique_ptr<Statement> parse_extern_declaration(Visibility visibility) {
        auto position = current_token.position;
        advance();

        if (check(TokenType::Fn)) {
            advance();
            auto prototype = parse_function_prototype();
            match(TokenType::Semicolon);
            return std::make_unique<ExternFunctionDeclaration>(position, visibility,
                                                               std::move(prototype));
        }

        if (check(TokenType::Var)) {
            advance();
            auto name = std::string(expect(TokenType::Identifier).value);
            expect(TokenType::Colon);
            auto type = parse_type_expression();
            match(TokenType::Semicolon);
            return std::make_unique<ExternVarDeclaration>(position, visibility, std::move(name),
                                                          std::move(type));
        }

        logger.error(position, "expected 'fn' or 'var' after 'extern'");
    }

    std::unique_ptr<ImportStatement> parse_import_statement() {
        auto position = current_token.position;
        advance();

        auto path = std::string(expect(TokenType::Identifier).value);
        while (match(TokenType::Dot)) {
            path += ".";
            path += std::string(expect(TokenType::Identifier).value);
        }

        match(TokenType::Semicolon);
        return std::make_unique<ImportStatement>(position, std::move(path));
    }

    std::unique_ptr<Statement> parse_statement() {
        if (check(TokenType::Import)) {
            return parse_import_statement();
        }

        auto visibility = Visibility::Public;
        if (check(TokenType::Public) || check(TokenType::Private)) {
            visibility = parse_visibility();
        }

        if (check(TokenType::Extern)) {
            return parse_extern_declaration(visibility);
        }
        if (check(TokenType::Fn)) {
            return parse_function_declaration(visibility);
        }
        if (check(TokenType::Struct)) {
            return parse_struct_declaration(visibility);
        }
        if (check(TokenType::Var) || check(TokenType::Const)) {
            return parse_var_declaration(visibility);
        }
        if (check(TokenType::Return)) {
            return parse_return_statement();
        }
        if (check(TokenType::If)) {
            return parse_if_statement();
        }
        if (check(TokenType::LeftBrace)) {
            return parse_block_statement();
        }

        return parse_expression_statement();
    }

  public:
    Parser(Lexer lexer, Logger logger)
        : lexer(std::move(lexer)), logger(std::move(logger)),
          current_token(this->lexer.next_token()), peek_token(this->lexer.next_token()) {}

    Program parse() {
        std::vector<std::unique_ptr<Statement>> statements;
        while (!check(TokenType::Eof)) {
            statements.push_back(parse_statement());
        }
        return Program(std::move(statements));
    }
};
