module;

#include <cstdint>

export module zep.frontend.parser.precedence;

import zep.frontend.token.type;

export enum class Precedence : int {
    None = 0,
    Assignment = 1,
    Or = 2,
    And = 3,
    Equality = 4,
    Comparison = 5,
    Is = 6,
    As = 7,
    Additive = 8,
    Multiplicative = 9,
    Unary = 10,
    Postfix = 11,
};

export constexpr Precedence get_precedence(TokenType type) {
    switch (type) {
    case TokenType::Assign:
        return Precedence::Assignment;
    case TokenType::Or:
        return Precedence::Or;
    case TokenType::And:
        return Precedence::And;
    case TokenType::Equals:
    case TokenType::NotEquals:
        return Precedence::Equality;
    case TokenType::LessThan:
    case TokenType::GreaterThan:
    case TokenType::LessEqual:
    case TokenType::GreaterEqual:
        return Precedence::Comparison;
    case TokenType::Is:
        return Precedence::Is;
    case TokenType::As:
        return Precedence::As;
    case TokenType::Plus:
    case TokenType::Minus:
        return Precedence::Additive;
    case TokenType::Asterisk:
    case TokenType::Divide:
    case TokenType::Modulo:
        return Precedence::Multiplicative;
    case TokenType::LeftParen:
    case TokenType::LeftBracket:
    case TokenType::Dot:
        return Precedence::Postfix;
    default:
        return Precedence::None;
    }
}
