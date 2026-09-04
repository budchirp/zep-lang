module;

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.parser;

import zep.common.source.span;
import zep.common.target;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.token;
import zep.frontend.lexer;
import zep.frontend.parser.precedence;
import zep.common.context;
import zep.frontend.sema.context;
import zep.frontend.sema.type;
import zep.frontend.sema.kind;
import zep.frontend.sema.resolver.attribute;
import zep.frontend.sema.resolver.builtin;

export class Parser {
  private:
    class ParseFailure {
      public:
        ParseFailure() = default;
    };

    Context& context;
    SemaContext& sema;

    Lexer lexer;

    Token current_token;
    Token peek_token;
    bool stop_expression_at_greater = false;

    class NominalHeader {
      public:
        Span span;
        std::string name;
        std::vector<GenericParameter*> generic_parameters;

        explicit NominalHeader(Span span, std::string name,
                               std::vector<GenericParameter*> generic_parameters)
            : span(span), name(std::move(name)), generic_parameters(std::move(generic_parameters)) {
        }
    };

    static bool is_target_active(const std::vector<Attribute*>& attributes,
                                 const TargetInfo& target) {
        return AttributeResolver::is_target_active(attributes, target);
    }

    [[noreturn]] void error(Span span, std::string message) {
        context.diagnostics.add_error(span, std::move(message));
        throw ParseFailure();
    }

    void synchronize() {
        while (!check(Token::Type::Eof)) {
            if (check(Token::Type::Semicolon) || check(Token::Type::RightBrace)) {
                advance();
                return;
            }

            advance();
        }
    }

    void synchronize_statement() {
        while (!check(Token::Type::Eof)) {
            if (check(Token::Type::RightBrace)) {
                return;
            }

            if (check(Token::Type::Semicolon)) {
                advance();
                return;
            }

            advance();
        }
    }

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
            error(current_token.span,
                  "unexpected token '" + std::string(current_token.value) + "'");
        }

        auto token = current_token;

        advance();

        return token;
    }

    Token expect_path_identifier() {
        if (!check(Token::Type::Identifier) && !check(Token::Type::Type)) {
            error(current_token.span,
                  "unexpected token '" + std::string(current_token.value) + "'");
        }

        auto token = current_token;

        advance();

        return token;
    }

    const Type* resolve_primitive(const std::string& name) {
        if (name == "void") {
            return sema.builtin_resolver.primitives.at("void");
        }

        if (sema.builtin_resolver.primitives.contains(name)) {
            return sema.builtin_resolver.primitives.at(name);
        }

        return sema.types.create<NamedType>(name, std::vector<GenericArgumentType>{});
    }

    BuiltinCall* parse_builtin_call(Span span) {
        advance();

        auto builtin_name = std::string(expect(Token::Type::Identifier).value);

        if (!sema.builtin_resolver.is_builtin(builtin_name)) {
            error(span, "unknown builtin function '#" + builtin_name + "'");
        }

        expect(Token::Type::LeftParen);

        TypeExpression* type_argument = nullptr;
        std::vector<Expression*> arguments;

        if (builtin_name == "length" || builtin_name == "asm") {
            arguments.push_back(parse_expression());
        } else {
            type_argument = parse_type_expression();
        }

        expect(Token::Type::RightParen);

        return sema.nodes.create<BuiltinCall>(span, std::move(builtin_name), type_argument,
                                              std::move(arguments));
    }

    Expression* parse_call_suffix_if_any(Expression* callee, bool allow_paren_without_generics) {
        if (check(Token::Type::LessThan) && is_generic_start()) {
            auto generic_arguments = parse_generic_arguments();

            return parse_call_expression(callee, std::move(generic_arguments));
        }

        if (allow_paren_without_generics && check(Token::Type::LeftParen)) {
            return parse_call_expression(callee);
        }

        return nullptr;
    }

    Statement* parse_statement() {
        auto attributes = parse_attributes();

        auto visibility = parse_visibility();

        switch (current_token.type) {
        case Token::Type::Import:
            return parse_import_statement(visibility);
        case Token::Type::Extern:
            return parse_extern_declaration(visibility, std::move(attributes));
        case Token::Type::Fn:
        case Token::Type::Static:
            return parse_function_declaration(visibility, std::move(attributes));
        case Token::Type::Interface:
            return parse_interface_declaration(visibility, std::move(attributes));
        case Token::Type::Struct:
            return parse_struct_declaration(visibility, std::move(attributes));
        case Token::Type::Enum:
            return parse_enum_declaration(visibility, std::move(attributes));
        case Token::Type::Type:
            return parse_type_alias_declaration(visibility);
        case Token::Type::Var:
            return parse_var_declaration(visibility, std::move(attributes));
        case Token::Type::Return:
            return parse_return_statement();
        case Token::Type::While:
            return parse_while_statement();
        case Token::Type::For:
            return parse_for_statement();
        case Token::Type::Defer:
            return parse_defer_statement();
        default: {
            return sema.nodes.create<ExpressionStatement>(parse_expression());
        }
        }
    }

    Expression* parse_expression(Precedence::Type precedence = Precedence::Type::None,
                                 bool allow_arrow_access = true) {
        auto* left = parse_prefix();

        while (!(check(Token::Type::Eof) || check(Token::Type::Semicolon) ||
                 check(Token::Type::Comma) || check(Token::Type::RightParen) ||
                 check(Token::Type::RightBracket) || check(Token::Type::RightBrace) ||
                 check(Token::Type::Else) ||
                 (stop_expression_at_greater && check(Token::Type::GreaterThan)))) {
            if (allow_arrow_access && check(Token::Type::Arrow) && peek(Token::Type::Identifier)) {
                left = parse_postfix(left);
                continue;
            }

            if (Precedence::get(current_token.type) <= precedence) {
                break;
            }

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

            return sema.nodes.create<StringLiteral>(span, std::move(value));
        }
        case Token::Type::Char: {
            auto value = current_token.value.empty()
                             ? std::uint8_t{0}
                             : static_cast<std::uint8_t>(current_token.value.front());
            advance();

            return sema.nodes.create<CharLiteral>(span, value);
        }
        case Token::Type::Number: {
            auto value = std::string(current_token.value);
            advance();

            return sema.nodes.create<NumberLiteral>(span, std::move(value));
        }
        case Token::Type::Float: {
            auto value = std::string(current_token.value);
            advance();

            return sema.nodes.create<FloatLiteral>(span, std::move(value));
        }
        case Token::Type::Boolean: {
            auto value = current_token.value == "true";
            advance();

            return sema.nodes.create<BooleanLiteral>(span, value);
        }
        case Token::Type::Null: {
            advance();
            return sema.nodes.create<NullLiteral>(span);
        }
        case Token::Type::Identifier: {
            auto name = std::string(current_token.value);
            advance();

            auto generic_arguments = std::vector<GenericArgument*>{};
            if (check(Token::Type::LessThan) && is_generic_start()) {
                generic_arguments = parse_generic_arguments();

                if (check(Token::Type::LeftParen)) {
                    auto* callee = sema.nodes.create<IdentifierExpression>(span, std::move(name));
                    return parse_call_expression(callee, std::move(generic_arguments));
                }

                if (check(Token::Type::LeftBrace)) {
                    auto* type_name =
                        sema.nodes.create<IdentifierExpression>(span, std::move(name));
                    return parse_struct_literal(type_name, std::move(generic_arguments));
                }

                if (check(Token::Type::DoubleColon)) {
                    return parse_qualified_access_expression(std::move(name),
                                                             std::move(generic_arguments), span);
                }

                if (check(Token::Type::Dot)) {
                    return sema.nodes.create<IdentifierExpression>(span, std::move(name),
                                                                   std::move(generic_arguments));
                }

                error(current_token.span, "expected '::', '.', '(' or '{' after generic arguments");
            }

            if (check(Token::Type::DoubleColon)) {
                return parse_qualified_access_expression(std::move(name), {}, span);
            }

            if (check(Token::Type::LeftBrace)) {
                auto* type_name = sema.nodes.create<IdentifierExpression>(span, std::move(name));
                return parse_struct_literal(type_name, {});
            }

            return sema.nodes.create<IdentifierExpression>(span, std::move(name));
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
        case Token::Type::When: {
            return parse_when_expression();
        }
        case Token::Type::LeftBracket: {
            return parse_array_literal();
        }
        case Token::Type::LeftBrace: {
            return parse_closure_expression();
        }
        case Token::Type::Do:
            return parse_block_expression();
        case Token::Type::Hash:
            return parse_builtin_call(span);
        default:
            error(span,
                  "unexpected token '" + std::string(current_token.value) + "' in expression");
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
            return sema.nodes.create<AssignExpression>(span, left, right);
        }
        case Token::Type::LeftParen:
        case Token::Type::LeftBracket:
        case Token::Type::Dot:
        case Token::Type::Arrow:
            return parse_postfix(left);
        default:
            error(current_token.span,
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
        case Token::Type::Arrow:
        case Token::Type::Dot: {
            auto* member = parse_member_expression(left, current_token.type != Token::Type::Dot);

            if (auto* call = parse_call_suffix_if_any(member, true); call != nullptr) {
                return call;
            }

            return member;
        }
        default:
            error(current_token.span,
                  "unexpected token '" + std::string(current_token.value) + "'");
        }
    }

    template <typename Item, typename ParseItem>
    std::vector<Item*> parse_comma_list(Token::Type end, std::size_t reserve_size,
                                        ParseItem parse_item) {
        std::vector<Item*> items;
        items.reserve(reserve_size);

        while (!check(end) && !check(Token::Type::Eof)) {
            items.push_back(parse_item());

            if (!check(end)) {
                expect(Token::Type::Comma);
            }
        }

        return items;
    }

    Visibility::Type parse_visibility() {
        switch (current_token.type) {
        case Token::Type::Public:
            advance();

            return Visibility::Type::Public;
        case Token::Type::Private:
            advance();

            return Visibility::Type::Private;
        default:
            return Visibility::Type::Private;
        }
    }

    NominalHeader parse_nominal_header(Token::Type keyword) {
        auto span = current_token.span;

        expect(keyword);

        auto name = std::string(expect(Token::Type::Identifier).value);

        auto generic_parameters = std::vector<GenericParameter*>{};
        if (check(Token::Type::LessThan)) {
            generic_parameters = parse_generic_parameters();
        }

        return NominalHeader(span, std::move(name), std::move(generic_parameters));
    }

    FunctionDeclaration::MemberModifiers parse_member_modifiers(const std::string& parent
                                                                [[maybe_unused]]) {
        auto modifiers = FunctionDeclaration::MemberModifiers(current_token.span);

        while (check(Token::Type::Static) || check(Token::Type::Override)) {
            if (match(Token::Type::Static)) {
                modifiers.is_static = true;
                modifiers.span = current_token.span;
                continue;
            }

            if (match(Token::Type::Override)) {
                modifiers.is_override = true;
                modifiers.span = current_token.span;
            }
        }

        return modifiers;
    }

    Field* parse_field(Visibility::Type visibility, std::vector<Attribute*> attributes = {}) {
        auto field_span = current_token.span;

        auto field_name = std::string(expect(Token::Type::Identifier).value);
        expect(Token::Type::Colon);
        auto* type_expression = parse_type_expression();

        Expression* default_value = nullptr;
        if (match(Token::Type::Assign)) {
            default_value = parse_expression();
        }

        return sema.nodes.create<Field>(field_span, visibility, std::move(attributes),
                                        std::move(field_name), type_expression, default_value);
    }

    Field* parse_stored_field(Visibility::Type visibility,
                              std::vector<Attribute*> attributes = {}) {
        expect(Token::Type::Var);

        return parse_field(visibility, std::move(attributes));
    }

    bool parse_visibility_section(Visibility::Type& visibility) {
        if (!check(Token::Type::Public) && !check(Token::Type::Private)) {
            return false;
        }

        auto next_visibility = current_token.type == Token::Type::Public
                                   ? Visibility::Type::Public
                                   : Visibility::Type::Private;

        advance();

        if (!match(Token::Type::Colon)) {
            error(current_token.span, "visibility in nominal bodies must use sections");
            visibility = next_visibility;
            return false;
        }

        visibility = next_visibility;
        return true;
    }

    std::vector<Attribute*> parse_attributes() {
        auto attributes = std::vector<Attribute*>{};

        while (check(Token::Type::At)) {
            auto span = current_token.span;
            advance();

            auto name = std::string(expect(Token::Type::Identifier).value);

            auto arguments = std::vector<Expression*>{};
            if (match(Token::Type::LeftParen)) {
                arguments = parse_comma_list<Expression>(Token::Type::RightParen, 0,
                                                         [&]() { return parse_expression(); });

                expect(Token::Type::RightParen);
            }

            attributes.push_back(
                sema.nodes.create<Attribute>(span, std::move(name), std::move(arguments)));
        }

        return attributes;
    }

    std::vector<Parameter*> parse_parameters(bool& is_variadic) {
        std::vector<Parameter*> parameters;
        parameters.reserve(4);
        while (!check(Token::Type::RightParen) && !check(Token::Type::Eof)) {
            if (match(Token::Type::Ellipsis)) {
                is_variadic = true;
                break;
            }

            auto span = current_token.span;
            auto attributes = parse_attributes();
            auto name = std::string(expect(Token::Type::Identifier).value);
            expect(Token::Type::Colon);
            auto* type = parse_type_expression();

            parameters.push_back(
                sema.nodes.create<Parameter>(span, std::move(name), type, std::move(attributes)));

            if (!check(Token::Type::RightParen)) {
                expect(Token::Type::Comma);
            }
        }

        return parameters;
    }

    std::vector<Argument*> parse_arguments() {
        return parse_comma_list<Argument>(Token::Type::RightParen, 4, [&]() {
            auto span = current_token.span;

            auto name = std::string();
            if (check(Token::Type::Identifier) && peek(Token::Type::Colon)) {
                name = std::string(current_token.value);
                advance();
                expect(Token::Type::Colon);
            }

            auto* value = parse_expression();

            return sema.nodes.create<Argument>(span, std::move(name), value);
        });
    }

    bool generic_argument_has_expression_operator() {
        auto saved_current = current_token;
        auto saved_peek = peek_token;
        auto saved_checkpoint = lexer.take_checkpoint();

        auto depth = 0;
        auto result = false;

        while (!check(Token::Type::Eof)) {
            if (depth == 0 && (check(Token::Type::Comma) || check(Token::Type::GreaterThan))) {
                break;
            }

            if (check(Token::Type::LeftParen)) {
                result = true;
                depth = depth + 1;
            } else if (check(Token::Type::LeftBracket) || check(Token::Type::LeftBrace) ||
                       check(Token::Type::LessThan)) {
                depth = depth + 1;
            } else if (check(Token::Type::RightParen) || check(Token::Type::RightBracket) ||
                       check(Token::Type::RightBrace) ||
                       (depth > 0 && check(Token::Type::GreaterThan))) {
                if (depth > 0) {
                    depth = depth - 1;
                }
            } else if (check(Token::Type::Plus) || check(Token::Type::Minus) ||
                       check(Token::Type::Asterisk) || check(Token::Type::Divide) ||
                       check(Token::Type::Modulo) || check(Token::Type::Equals) ||
                       check(Token::Type::NotEquals) || check(Token::Type::LessEqual) ||
                       check(Token::Type::GreaterEqual) || check(Token::Type::And) ||
                       check(Token::Type::Or) || check(Token::Type::As) || check(Token::Type::Is)) {
                result = true;
                break;
            }

            advance();
        }

        current_token = saved_current;
        peek_token = saved_peek;
        lexer.restore_checkpoint(saved_checkpoint);

        return result;
    }

    bool generic_argument_starts_expression() {
        switch (current_token.type) {
        case Token::Type::Number:
        case Token::Type::String:
        case Token::Type::Char:
        case Token::Type::Boolean:
        case Token::Type::Plus:
        case Token::Type::Minus:
        case Token::Type::Not:
        case Token::Type::Hash:
            return true;
        case Token::Type::Identifier:
            return generic_argument_has_expression_operator();
        case Token::Type::LeftParen: {
            auto saved_current = current_token;
            auto saved_peek = peek_token;
            auto saved_checkpoint = lexer.take_checkpoint();
            advance();
            const auto result = generic_argument_starts_expression();
            current_token = saved_current;
            peek_token = saved_peek;
            lexer.restore_checkpoint(saved_checkpoint);
            return result;
        }
        default:
            return false;
        }
    }

    Expression* parse_generic_argument_expression() {
        auto saved = stop_expression_at_greater;
        stop_expression_at_greater = true;
        auto* expression = parse_expression();
        stop_expression_at_greater = saved;

        return expression;
    }

    std::vector<GenericParameter*> parse_generic_parameters() {
        expect(Token::Type::LessThan);

        auto generic_parameters =
            parse_comma_list<GenericParameter>(Token::Type::GreaterThan, 2, [&]() {
                auto span = current_token.span;

                if (match(Token::Type::Const)) {
                    auto name = std::string(expect(Token::Type::Identifier).value);
                    expect(Token::Type::Colon);
                    auto* value_type = parse_type_expression();

                    return sema.nodes.create<GenericParameter>(
                        span, GenericParameterType::Kind::Type::Const, std::move(name), value_type);
                }

                auto name = std::string(expect(Token::Type::Identifier).value);

                TypeExpression* constraint = nullptr;
                if (match(Token::Type::Colon)) {
                    constraint = parse_type_expression();
                }

                return sema.nodes.create<GenericParameter>(span, std::move(name), constraint);
            });

        expect(Token::Type::GreaterThan);

        return generic_parameters;
    }

    std::vector<GenericArgument*> parse_generic_arguments() {
        expect(Token::Type::LessThan);

        auto generic_arguments =
            parse_comma_list<GenericArgument>(Token::Type::GreaterThan, 2, [&]() {
                auto span = current_token.span;

                auto name = std::string();
                if (check(Token::Type::Identifier) && peek(Token::Type::Colon)) {
                    name = std::string(current_token.value);
                    advance();
                    expect(Token::Type::Colon);
                }

                if (generic_argument_starts_expression()) {
                    auto* value = parse_generic_argument_expression();

                    return sema.nodes.create<GenericArgument>(span, std::move(name), value);
                }

                auto* type_expression = parse_type_expression();

                return sema.nodes.create<GenericArgument>(span, std::move(name), type_expression);
            });

        expect(Token::Type::GreaterThan);

        return generic_arguments;
    }

    TypeExpression* parse_type_expression() {
        auto span = current_token.span;

        if (check(Token::Type::LeftParen)) {
            advance();

            auto parameter_types = parse_comma_list<TypeExpression>(
                Token::Type::RightParen, 0, [&]() { return parse_type_expression(); });

            expect(Token::Type::RightParen);

            if (!match(Token::Type::Arrow)) {
                if (parameter_types.size() == 1) {
                    return parameter_types[0];
                }

                error(span, "expected '->' after function type parameters");
                return parameter_types.empty() ? nullptr : parameter_types[0];
            }

            auto* return_type_expression = parse_type_expression();

            auto params = std::vector<ParameterType>{};
            params.reserve(parameter_types.size());
            for (auto* type_expression : parameter_types) {
                params.emplace_back("", type_expression->type);
            }

            const auto* function_type =
                sema.types.create<FunctionType>("", return_type_expression->type, std::move(params),
                                                std::vector<GenericParameterType>{}, false);

            auto* expression = sema.nodes.create<TypeExpression>(span, function_type);
            expression->parameters = std::move(parameter_types);
            expression->return_type = return_type_expression;
            return expression;
        }

        if (check(Token::Type::Asterisk)) {
            advance();

            auto is_mutable = false;
            if (check(Token::Type::Mut)) {
                is_mutable = true;
                advance();
            }

            auto* element = parse_type_expression();

            const auto* pointer_type = sema.types.create<PointerType>(element->type, is_mutable);
            auto* expression = sema.nodes.create<TypeExpression>(span, pointer_type);
            expression->element = element;
            return expression;
        }

        auto name = std::string(expect_path_identifier().value);
        while (check(Token::Type::Dot) || check(Token::Type::DoubleColon)) {
            auto separator = std::string(current_token.value);
            advance();

            name += separator;
            name += std::string(expect_path_identifier().value);
        }

        const auto* type = resolve_primitive(name);
        std::vector<GenericArgument*> generic_arguments;

        if (check(Token::Type::LessThan)) {
            if (type->kind == Type::Kind::Type::Named) {
                generic_arguments = parse_generic_arguments();
            }
        }

        std::vector<Expression*> array_sizes;

        while (check(Token::Type::LeftBracket)) {
            advance();

            Expression* size_expression = nullptr;
            if (!check(Token::Type::RightBracket)) {
                size_expression = parse_expression();
            }

            expect(Token::Type::RightBracket);

            array_sizes.push_back(size_expression);
            type = sema.types.create<ArrayType>(type, UnsizedArrayExtent());
        }

        auto* expression = sema.nodes.create<TypeExpression>(span, type, std::move(array_sizes));
        expression->generic_arguments = std::move(generic_arguments);
        return expression;
    }

    Expression* parse_unary_expression() {
        auto span = current_token.span;

        auto op = UnaryExpression::Operator::from_token(current_token.type);
        if (!op.has_value()) {
            error(span, "unexpected token '" + std::string(current_token.value) +
                            "' in unary expression");
        }

        auto op_value = *op;

        if (op_value == UnaryExpression::Operator::Type::AddressOf) {
            advance();

            op_value = match(Token::Type::Mut) ? UnaryExpression::Operator::Type::AddressOfMut
                                               : UnaryExpression::Operator::Type::AddressOf;
        } else {
            advance();
        }

        auto* operand = parse_expression(Precedence::Type::Unary);
        return sema.nodes.create<UnaryExpression>(span, op_value, operand);
    }

    Expression* parse_binary_expression(Expression* left) {
        auto span = current_token.span;

        auto op = BinaryExpression::Operator::from_token(current_token.type);
        if (!op.has_value()) {
            error(span, "unexpected token '" + std::string(current_token.value) +
                            "' in binary expression");
        }

        auto op_value = *op;

        auto precedence = Precedence::get(current_token.type);

        advance();

        if (op_value == BinaryExpression::Operator::Type::As ||
            op_value == BinaryExpression::Operator::Type::Is) {
            auto* right = parse_type_expression();
            return sema.nodes.create<BinaryExpression>(span, left, op_value, right);
        }

        auto* right = parse_expression(precedence);
        return sema.nodes.create<BinaryExpression>(span, left, op_value, right);
    }

    IfExpression* parse_if_expression() {
        auto span = current_token.span;

        expect(Token::Type::If);

        expect(Token::Type::LeftParen);
        auto* condition = parse_expression();
        expect(Token::Type::RightParen);

        auto* then_branch = parse_block_statement();

        Statement* else_branch = nullptr;
        if (match(Token::Type::Else)) {
            if (check(Token::Type::If)) {
                else_branch = sema.nodes.create<ExpressionStatement>(parse_if_expression());
            } else {
                else_branch = parse_block_statement();
            }
        }

        return sema.nodes.create<IfExpression>(span, condition, then_branch, else_branch);
    }

    Statement* parse_when_arm_body() {
        if (check(Token::Type::LeftBrace)) {
            return parse_block_statement();
        }

        return sema.nodes.create<ExpressionStatement>(parse_expression());
    }

    WhenPattern* parse_when_pattern() {
        auto span = current_token.span;

        if (check(Token::Type::Identifier) && peek(Token::Type::DoubleColon)) {
            auto enum_name = std::string(current_token.value);
            advance();
            expect(Token::Type::DoubleColon);

            auto variant_name = std::string(expect(Token::Type::Identifier).value);

            auto fields = std::vector<WhenPatternField*>{};

            if (match(Token::Type::LeftBrace)) {
                fields = parse_comma_list<WhenPatternField>(Token::Type::RightBrace, 4, [&]() {
                    auto field_span = current_token.span;

                    auto field_name = std::string(expect(Token::Type::Identifier).value);
                    auto binding_name = field_name;

                    if (match(Token::Type::Colon)) {
                        binding_name = std::string(expect(Token::Type::Identifier).value);
                    }

                    return sema.nodes.create<WhenPatternField>(field_span, std::move(field_name),
                                                               std::move(binding_name));
                });

                expect(Token::Type::RightBrace);
            }

            auto* enum_identifier =
                sema.nodes.create<IdentifierExpression>(span, std::move(enum_name));

            return sema.nodes.create<WhenPattern>(span, enum_identifier,
                                                  std::vector<GenericArgument*>{},
                                                  std::move(variant_name), std::move(fields));
        }

        return sema.nodes.create<WhenPattern>(span,
                                              parse_expression(Precedence::Type::None, false));
    }

    WhenArm* parse_when_arm() {
        auto span = current_token.span;

        if (match(Token::Type::Else)) {
            expect(Token::Type::Arrow);
            auto* body = parse_when_arm_body();

            return sema.nodes.create<WhenArm>(span, true, std::vector<WhenPattern*>{}, nullptr,
                                              body);
        }

        auto patterns = std::vector<WhenPattern*>{};

        patterns.push_back(parse_when_pattern());
        while (match(Token::Type::Comma)) {
            patterns.push_back(parse_when_pattern());
        }

        Expression* guard = nullptr;
        if (match(Token::Type::If)) {
            expect(Token::Type::LeftParen);
            guard = parse_expression();
            expect(Token::Type::RightParen);
        }

        expect(Token::Type::Arrow);
        auto* body = parse_when_arm_body();

        return sema.nodes.create<WhenArm>(span, false, std::move(patterns), guard, body);
    }

    WhenExpression* parse_when_expression() {
        auto span = current_token.span;

        expect(Token::Type::When);

        Expression* subject = nullptr;
        if (match(Token::Type::LeftParen)) {
            subject = parse_expression();
            expect(Token::Type::RightParen);
        }

        expect(Token::Type::LeftBrace);

        auto arms = parse_comma_list<WhenArm>(Token::Type::RightBrace, 4,
                                              [&]() { return parse_when_arm(); });

        expect(Token::Type::RightBrace);

        return sema.nodes.create<WhenExpression>(span, subject, std::move(arms));
    }

    StructLiteralExpression*
    parse_struct_literal(Expression* name, std::vector<GenericArgument*> generic_arguments = {}) {
        auto span = name->span;

        expect(Token::Type::LeftBrace);

        auto fields = parse_comma_list<StructLiteralField>(Token::Type::RightBrace, 4, [&]() {
            auto field_span = current_token.span;

            auto field_name = std::string(expect(Token::Type::Identifier).value);
            expect(Token::Type::Colon);
            auto* value = parse_expression();

            return sema.nodes.create<StructLiteralField>(field_span, std::move(field_name), value);
        });

        expect(Token::Type::RightBrace);

        return sema.nodes.create<StructLiteralExpression>(span, name, std::move(generic_arguments),
                                                          std::move(fields));
    }

    CallExpression* parse_call_expression(Expression* callee,
                                          std::vector<GenericArgument*> generic_arguments = {}) {
        auto span = current_token.span;

        expect(Token::Type::LeftParen);
        auto arguments = parse_arguments();
        expect(Token::Type::RightParen);

        return sema.nodes.create<CallExpression>(span, callee, std::move(generic_arguments),
                                                 std::move(arguments));
    }

    IndexExpression* parse_index_expression(Expression* value) {
        auto span = current_token.span;

        expect(Token::Type::LeftBracket);
        auto* index = parse_expression();
        expect(Token::Type::RightBracket);

        return sema.nodes.create<IndexExpression>(span, value, index);
    }

    MemberExpression* parse_member_expression(Expression* value, bool arrow_access = false) {
        auto span = current_token.span;

        expect(arrow_access ? Token::Type::Arrow : Token::Type::Dot);
        auto member = std::string();

        if (match(Token::Type::Tilde)) {
            member = "~";
        }

        member += std::string(expect_path_identifier().value);

        return sema.nodes.create<MemberExpression>(
            span,
            arrow_access ? sema.nodes.create<UnaryExpression>(
                               span, UnaryExpression::Operator::Type::Dereference, value)
                         : value,
            std::move(member));
    }

    Expression* parse_qualified_access_expression(
        std::string parent, std::vector<GenericArgument*> parent_generic_arguments, Span span) {
        expect(Token::Type::DoubleColon);

        auto member = std::string(expect(Token::Type::Identifier).value);

        if (check(Token::Type::LeftBrace)) {
            auto* qualified = sema.nodes.create<QualifiedAccessExpression>(
                span, std::move(parent), std::move(parent_generic_arguments), std::move(member));
            return parse_struct_literal(qualified, {});
        }

        auto* qualified = sema.nodes.create<QualifiedAccessExpression>(
            span, std::move(parent), std::move(parent_generic_arguments), std::move(member));

        if (auto* call = parse_call_suffix_if_any(qualified, false); call != nullptr) {
            return call;
        }

        return qualified;
    }

    EnumVariantExpression*
    parse_enum_variant_expression(IdentifierExpression* enum_name,
                                  std::vector<GenericArgument*> generic_arguments,
                                  std::string variant_name) {
        auto span = enum_name->span;

        auto has_payload = false;
        auto fields = std::vector<StructLiteralField*>{};
        fields.reserve(4);

        if (match(Token::Type::LeftBrace)) {
            has_payload = true;

            fields = parse_comma_list<StructLiteralField>(Token::Type::RightBrace, 4, [&]() {
                auto field_span = current_token.span;

                auto field_name = std::string(expect(Token::Type::Identifier).value);
                expect(Token::Type::Colon);
                auto* value = parse_expression();

                return sema.nodes.create<StructLiteralField>(field_span, std::move(field_name),
                                                             value);
            });

            expect(Token::Type::RightBrace);
        }

        return sema.nodes.create<EnumVariantExpression>(
            span, enum_name, std::move(generic_arguments), std::move(variant_name), has_payload,
            std::move(fields));
    }

    ImportStatement* parse_import_statement(Visibility::Type visibility) {
        auto span = current_token.span;

        if (visibility == Visibility::Type::Public) {
            error(span, "imports cannot be public");
        }

        expect(Token::Type::Import);

        auto path = std::vector<IdentifierExpression*>{};

        auto first = expect_path_identifier();
        path.push_back(
            sema.nodes.create<IdentifierExpression>(first.span, std::string(first.value)));

        while (match(Token::Type::Dot)) {
            auto part = expect_path_identifier();
            path.push_back(
                sema.nodes.create<IdentifierExpression>(part.span, std::string(part.value)));
        }

        if (path.size() < 2) {
            error(span, "import must name an exported symbol");
        }

        auto alias = std::string();
        if (match(Token::Type::As)) {
            alias = std::string(expect(Token::Type::Identifier).value);
        }

        return sema.nodes.create<ImportStatement>(span, std::move(path), std::move(alias));
    }

    std::vector<TypeExpression*> parse_inheritance_list() {
        auto inheritance = std::vector<TypeExpression*>{};

        if (!match(Token::Type::Colon)) {
            return inheritance;
        }

        inheritance = parse_comma_list<TypeExpression>(Token::Type::LeftBrace, 2,
                                                       [&]() { return parse_type_expression(); });

        return inheritance;
    }

    InterfaceDeclaration* parse_interface_declaration(Visibility::Type visibility,
                                                      std::vector<Attribute*> attributes = {}) {
        auto header = parse_nominal_header(Token::Type::Interface);
        auto inheritance = parse_inheritance_list();

        expect(Token::Type::LeftBrace);

        auto methods = std::vector<FunctionPrototype*>{};
        auto member_visibility = Visibility::Type::Private;

        while (!check(Token::Type::RightBrace) && !check(Token::Type::Eof)) {
            auto member_attributes = parse_attributes();

            if (parse_visibility_section(member_visibility)) {
                continue;
            }

            if (!member_attributes.empty()) {
                error(current_token.span, "attributes are not supported on interface methods");
            }

            if (member_visibility != Visibility::Type::Public) {
                error(current_token.span, "interface methods must be public");
            }

            if (!check(Token::Type::Fn)) {
                error(current_token.span, "interfaces can only contain method signatures");
            }

            expect(Token::Type::Fn);
            auto* prototype = parse_function_prototype();
            if (prototype->receiver_kind == FunctionPrototype::ReceiverKind::Type::None) {
                prototype->receiver_kind = FunctionPrototype::ReceiverKind::Type::Borrow;
            }
            methods.push_back(prototype);
        }

        expect(Token::Type::RightBrace);

        return sema.nodes.create<InterfaceDeclaration>(
            header.span, visibility, std::move(header.name), std::move(attributes),
            std::move(header.generic_parameters), std::move(inheritance), std::move(methods));
    }

    StructDeclaration* parse_struct_declaration(Visibility::Type visibility,
                                                std::vector<Attribute*> attributes = {}) {
        auto header = parse_nominal_header(Token::Type::Struct);

        auto inheritance = parse_inheritance_list();

        expect(Token::Type::LeftBrace);

        auto fields = std::vector<Field*>{};
        auto methods = std::vector<FunctionDeclaration*>{};
        auto structs = std::vector<StructDeclaration*>{};
        auto enums = std::vector<EnumDeclaration*>{};
        auto member_visibility = Visibility::Type::Private;

        while (!check(Token::Type::RightBrace) && !check(Token::Type::Eof)) {
            auto member_attributes = parse_attributes();

            if (parse_visibility_section(member_visibility)) {
                continue;
            }

            if (try_parse_member_method(methods, member_visibility, member_attributes,
                                        header.name)) {
                continue;
            }

            if (check(Token::Type::Struct)) {
                structs.push_back(
                    parse_struct_declaration(member_visibility, std::move(member_attributes)));
                continue;
            }

            if (check(Token::Type::Enum)) {
                enums.push_back(
                    parse_enum_declaration(member_visibility, std::move(member_attributes)));
                continue;
            }

            if (!check(Token::Type::Var)) {
                error(current_token.span, "struct fields must be declared with 'var'");
            }

            fields.push_back(parse_stored_field(member_visibility, std::move(member_attributes)));
        }

        expect(Token::Type::RightBrace);

        return sema.nodes.create<StructDeclaration>(
            header.span, visibility, std::move(header.name), std::move(attributes),
            std::move(header.generic_parameters), std::move(inheritance), std::move(fields),
            std::move(methods), std::move(structs), std::move(enums));
    }

    EnumDeclaration* parse_enum_declaration(Visibility::Type visibility,
                                            std::vector<Attribute*> attributes = {}) {
        auto header = parse_nominal_header(Token::Type::Enum);

        auto entries = parse_inheritance_list();
        auto* backing_type = static_cast<TypeExpression*>(nullptr);
        auto inheritance = std::vector<TypeExpression*>{};
        if (!entries.empty()) {
            backing_type = entries.front();
            inheritance.reserve(entries.size() - 1);

            for (std::size_t index = 1; index < entries.size(); ++index) {
                inheritance.push_back(entries[index]);
            }
        }

        expect(Token::Type::LeftBrace);

        auto variants = std::vector<EnumVariant*>{};
        auto methods = std::vector<FunctionDeclaration*>{};
        auto member_visibility = Visibility::Type::Private;

        while (!check(Token::Type::RightBrace) && !check(Token::Type::Eof)) {
            auto member_attributes = parse_attributes();

            if (parse_visibility_section(member_visibility)) {
                continue;
            }

            if (try_parse_member_method(methods, member_visibility, member_attributes,
                                        header.name)) {
                continue;
            }

            auto variant_span = current_token.span;
            auto variant_name = std::string(expect(Token::Type::Identifier).value);

            auto fields = std::vector<Field*>{};

            if (match(Token::Type::LeftBrace)) {
                while (!check(Token::Type::RightBrace) && !check(Token::Type::Eof)) {
                    auto field_attributes = parse_attributes();
                    fields.push_back(
                        parse_field(Visibility::Type::Public, std::move(field_attributes)));

                    if (!check(Token::Type::RightBrace)) {
                        expect(Token::Type::Comma);
                    }
                }

                expect(Token::Type::RightBrace);
            }

            Expression* value_expression = nullptr;
            if (match(Token::Type::Assign)) {
                value_expression = parse_expression();
            }

            variants.push_back(sema.nodes.create<EnumVariant>(
                variant_span, std::move(member_attributes), std::move(variant_name),
                std::move(fields), value_expression));
        }

        expect(Token::Type::RightBrace);

        return sema.nodes.create<EnumDeclaration>(
            header.span, visibility, std::move(header.name), std::move(attributes),
            std::move(header.generic_parameters), backing_type, std::move(inheritance),
            std::move(variants), std::move(methods));
    }

    TypeAliasDeclaration* parse_type_alias_declaration(Visibility::Type visibility) {
        auto header = parse_nominal_header(Token::Type::Type);

        expect(Token::Type::Assign);

        auto* target = parse_type_expression();

        return sema.nodes.create<TypeAliasDeclaration>(
            header.span, visibility, std::move(header.name), std::move(header.generic_parameters),
            target);
    }

    FunctionPrototype* parse_function_prototype(std::string* extension_parent = nullptr) {
        auto span = current_token.span;

        auto name = std::string();
        if (match(Token::Type::Tilde)) {
            name = "~";
        }

        name += std::string(expect(Token::Type::Identifier).value);

        if (extension_parent != nullptr && name != "~" && match(Token::Type::Dot)) {
            *extension_parent = std::move(name);
            name = std::string(expect(Token::Type::Identifier).value);
        }

        auto generic_parameters = std::vector<GenericParameter*>{};
        if (check(Token::Type::LessThan)) {
            generic_parameters = parse_generic_parameters();
        }

        expect(Token::Type::LeftParen);
        auto is_variadic = false;
        auto parameters = parse_parameters(is_variadic);
        expect(Token::Type::RightParen);

        auto is_mut = match(Token::Type::Mut);

        expect(Token::Type::Arrow);
        auto* return_type = parse_type_expression();

        auto* prototype = sema.nodes.create<FunctionPrototype>(span, is_variadic, std::move(name),
                                                               std::move(generic_parameters),
                                                               std::move(parameters), return_type);

        if (is_mut) {
            prototype->receiver_kind = FunctionPrototype::ReceiverKind::Type::MutBorrow;
        }

        return prototype;
    }

    ClosureExpression* parse_closure_expression() {
        auto span = current_token.span;

        expect(Token::Type::LeftBrace);

        auto parameters = std::vector<Parameter*>{};

        if (scan_for_arrow_in_closure()) {
            parameters = parse_comma_list<Parameter>(Token::Type::Arrow, 2, [&]() {
                auto parameter_span = current_token.span;
                auto parameter_name = std::string(expect(Token::Type::Identifier).value);

                TypeExpression* parameter_type = nullptr;
                if (match(Token::Type::Colon)) {
                    parameter_type = parse_type_expression();
                }

                return sema.nodes.create<Parameter>(parameter_span, std::move(parameter_name),
                                                    parameter_type);
            });

            expect(Token::Type::Arrow);
        }

        auto statements = std::vector<Statement*>{};
        while (!check(Token::Type::RightBrace) && !check(Token::Type::Eof)) {
            statements.push_back(parse_statement());

            if (check(Token::Type::Semicolon)) {
                advance();
            }
        }

        expect(Token::Type::RightBrace);

        auto* body = sema.nodes.create<BlockStatement>(span, std::move(statements));

        return sema.nodes.create<ClosureExpression>(span, std::move(parameters), body);
    }

    BlockExpression* parse_block_expression() {
        auto span = current_token.span;
        expect(Token::Type::Do);
        return sema.nodes.create<BlockExpression>(span, parse_block_statement());
    }

    ArrayLiteralExpression* parse_array_literal() {
        auto span = current_token.span;

        expect(Token::Type::LeftBracket);

        auto elements = std::vector<Expression*>{};
        if (!check(Token::Type::RightBracket)) {
            elements = parse_comma_list<Expression>(Token::Type::RightBracket, 0,
                                                    [&]() { return parse_expression(); });
        }

        expect(Token::Type::RightBracket);

        return sema.nodes.create<ArrayLiteralExpression>(span, std::move(elements));
    }

    bool scan_for_arrow_in_closure() {
        auto depth = 0;
        auto saved_current = current_token;
        auto saved_peek = peek_token;
        auto saved_checkpoint = lexer.take_checkpoint();

        while (!check(Token::Type::Eof)) {
            if (check(Token::Type::LeftBrace)) {
                depth = depth + 1;
            }

            if (check(Token::Type::RightBrace)) {
                if (depth == 0) {
                    current_token = saved_current;
                    peek_token = saved_peek;
                    lexer.restore_checkpoint(saved_checkpoint);
                    return false;
                }

                depth = depth - 1;
            }

            if (depth == 0 && check(Token::Type::Arrow)) {
                current_token = saved_current;
                peek_token = saved_peek;
                lexer.restore_checkpoint(saved_checkpoint);
                return true;
            }

            advance();
        }

        current_token = saved_current;
        peek_token = saved_peek;
        lexer.restore_checkpoint(saved_checkpoint);
        return false;
    }

    bool is_generic_start() {
        auto saved_current = current_token;
        auto saved_peek = peek_token;
        auto saved_checkpoint = lexer.take_checkpoint();

        auto depth = 0;
        auto found_greater = false;

        advance();

        while (!check(Token::Type::Eof)) {
            if (check(Token::Type::LessThan)) {
                depth = depth + 1;
            } else if (check(Token::Type::GreaterThan)) {
                if (depth == 0) {
                    found_greater = true;
                    advance();
                    break;
                }

                depth = depth - 1;
            } else if (check(Token::Type::Semicolon) || check(Token::Type::LeftBrace) ||
                       check(Token::Type::RightBrace)) {
                break;
            }

            advance();
        }

        auto result =
            found_greater && (check(Token::Type::LeftParen) || check(Token::Type::LeftBrace) ||
                              check(Token::Type::DoubleColon) || check(Token::Type::Dot));

        current_token = saved_current;
        peek_token = saved_peek;
        lexer.restore_checkpoint(saved_checkpoint);

        return result;
    }

    FunctionDeclaration* parse_function_declaration(Visibility::Type visibility,
                                                    std::vector<Attribute*> attributes = {},
                                                    const std::string& parent = {}) {
        auto modifiers = parse_member_modifiers(parent);

        expect(Token::Type::Fn);

        auto extension_parent = std::string();
        auto* prototype = parse_function_prototype(parent.empty() ? &extension_parent : nullptr);
        auto* body = parse_block_statement();
        auto actual_parent = parent.empty() ? extension_parent : parent;

        if (!actual_parent.empty() && !modifiers.is_static) {
            const auto constructor_match = prototype->name == actual_parent;
            const auto is_destructor = prototype->name == "~" + actual_parent;

            if (is_destructor ||
                (!constructor_match &&
                 prototype->receiver_kind == FunctionPrototype::ReceiverKind::Type::MutBorrow)) {
                prototype->receiver_kind = FunctionPrototype::ReceiverKind::Type::MutBorrow;
            } else if (!constructor_match) {
                prototype->receiver_kind = FunctionPrototype::ReceiverKind::Type::Borrow;
            }
        } else if (prototype->receiver_kind == FunctionPrototype::ReceiverKind::Type::MutBorrow) {
            error(prototype->span, "'mut' can only be used for methods");
        }

        auto span = body != nullptr ? Span(modifiers.span.start, body->span.end)
                                    : Span(modifiers.span.start, prototype->span.end);

        auto* declaration = sema.nodes.create<FunctionDeclaration>(
            span, visibility, std::move(attributes), modifiers.is_static,
            modifiers.is_override, !extension_parent.empty(), std::move(actual_parent), prototype,
            body);

        if (modifiers.is_static && declaration->parent.empty()) {
            error(declaration->span, "static functions require an extension target");
        }

        if (!is_target_active(declaration->attributes, sema.target)) {
            return nullptr;
        }

        return declaration;
    }

    bool try_parse_member_method(std::vector<FunctionDeclaration*>& methods,
                                 Visibility::Type visibility, std::vector<Attribute*> attributes,
                                 const std::string& parent_name) {
        if (!check(Token::Type::Fn) && !check(Token::Type::Static) &&
            !check(Token::Type::Override)) {
            return false;
        }

        if (auto* method =
                parse_function_declaration(visibility, std::move(attributes), parent_name);
            method != nullptr) {
            methods.push_back(method);
        }

        return true;
    }

    VarDeclaration* parse_var_declaration(Visibility::Type visibility,
                                          std::vector<Attribute*> attributes = {}) {
        auto span = current_token.span;

        auto storage_kind = StorageKind::Type::Var;
        advance();

        if (match(Token::Type::Mut)) {
            storage_kind = StorageKind::Type::VarMut;
        }

        auto name = std::string(expect(Token::Type::Identifier).value);

        TypeExpression* type = nullptr;
        if (match(Token::Type::Colon)) {
            type = parse_type_expression();
        }

        Expression* initializer = nullptr;
        if (match(Token::Type::Assign)) {
            initializer = parse_expression();
        }

        return sema.nodes.create<VarDeclaration>(span, visibility, storage_kind,
                                                 std::move(attributes), std::move(name), type,
                                                 initializer);
    }

    ReturnStatement* parse_return_statement() {
        auto span = current_token.span;

        expect(Token::Type::Return);

        Expression* value = nullptr;
        if (!check(Token::Type::Eof) && !check(Token::Type::Semicolon) &&
            !check(Token::Type::RightBrace)) {
            value = parse_expression();
        }

        return sema.nodes.create<ReturnStatement>(span, value);
    }

    DeferStatement* parse_defer_statement() {
        auto span = current_token.span;

        expect(Token::Type::Defer);

        auto* body = check(Token::Type::LeftBrace)
                         ? static_cast<Statement*>(parse_block_statement())
                         : parse_statement();

        return sema.nodes.create<DeferStatement>(span, body);
    }

    WhileStatement* parse_while_statement() {
        auto span = current_token.span;

        expect(Token::Type::While);
        expect(Token::Type::LeftParen);
        auto* condition = parse_expression();
        expect(Token::Type::RightParen);

        auto* body = parse_block_statement();

        return sema.nodes.create<WhileStatement>(span, condition, body);
    }

    Statement* parse_for_statement() {
        auto span = current_token.span;

        expect(Token::Type::For);
        expect(Token::Type::LeftParen);

        if (check(Token::Type::Var)) {
            auto* init_decl = parse_var_declaration(Visibility::Type::Private, {});
            expect(Token::Type::Semicolon);
            auto* condition = parse_expression();
            expect(Token::Type::Semicolon);
            auto* step = parse_expression();
            expect(Token::Type::RightParen);

            auto* body = parse_block_statement();

            return sema.nodes.create<ForStatement>(span, init_decl, condition, step, body);
        }

        auto* initializer = sema.nodes.create<ExpressionStatement>(parse_expression());
        expect(Token::Type::Semicolon);
        auto* condition = parse_expression();
        expect(Token::Type::Semicolon);
        auto* step = parse_expression();
        expect(Token::Type::RightParen);

        auto* body = parse_block_statement();

        return sema.nodes.create<ForStatement>(span, initializer, condition, step, body);
    }

    BlockStatement* parse_block_statement() {
        auto span = current_token.span;

        expect(Token::Type::LeftBrace);

        auto statements = std::vector<Statement*>{};
        statements.reserve(8);
        while (!check(Token::Type::RightBrace) && !check(Token::Type::Eof)) {
            try {
                if (auto* statement = parse_statement(); statement != nullptr) {
                    statements.push_back(statement);
                }
            } catch (const ParseFailure&) {
                synchronize_statement();
            }

            if (check(Token::Type::Semicolon)) {
                advance();
            }
        }

        auto right_brace = expect(Token::Type::RightBrace);

        return sema.nodes.create<BlockStatement>(Span(span.start, right_brace.span.end),
                                                std::move(statements));
    }

    Statement* parse_extern_declaration(Visibility::Type visibility,
                                        std::vector<Attribute*> attributes = {}) {
        auto span = current_token.span;

        expect(Token::Type::Extern);

        switch (current_token.type) {
        case Token::Type::Fn: {
            advance();

            auto* prototype = parse_function_prototype();
            auto* declaration = sema.nodes.create<ExternFunctionDeclaration>(
                span, visibility, std::move(attributes), prototype);

            if (!is_target_active(declaration->attributes, sema.target)) {
                return nullptr;
            }

            return declaration;
        }
        case Token::Type::Var: {
            advance();

            auto name = std::string(expect(Token::Type::Identifier).value);
            expect(Token::Type::Colon);
            auto* type_expression = parse_type_expression();

            return sema.nodes.create<ExternVarDeclaration>(span, visibility, std::move(attributes),
                                                           std::move(name), type_expression);
        }
        default:
            error(span, "expected 'fn' or 'var' after 'extern'");
            break;
        }
        return nullptr;
    }

  public:
    Parser(Context& context, SemaContext& sema, Lexer lexer)
        : context(context), sema(sema), lexer(std::move(lexer)),
          current_token(this->lexer.next_token()), peek_token(this->lexer.next_token()) {}

    Program parse() {
        Program program;

        while (!check(Token::Type::Eof)) {
            try {
                if (auto* statement = parse_statement(); statement != nullptr) {
                    program.statements.push_back(statement);
                }
            } catch (const ParseFailure&) {
                synchronize();
            }

            if (check(Token::Type::Semicolon)) {
                advance();
            }
        }

        return program;
    }
};
