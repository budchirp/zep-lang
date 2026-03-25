module;

#include <cstdint>

export module zep.frontend.parser.precedence;

import zep.frontend.token;

export class Precedence {
  public:
    enum class Type : std::uint8_t {
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
};

export constexpr Precedence::Type get_precedence(Token::Type type) {
    switch (type) {
    case Token::Type::Assign:
        return Precedence::Type::Assignment;
    case Token::Type::Or:
        return Precedence::Type::Or;
    case Token::Type::And:
        return Precedence::Type::And;
    case Token::Type::Equals:
    case Token::Type::NotEquals:
        return Precedence::Type::Equality;
    case Token::Type::LessThan:
    case Token::Type::GreaterThan:
    case Token::Type::LessEqual:
    case Token::Type::GreaterEqual:
        return Precedence::Type::Comparison;
    case Token::Type::Is:
        return Precedence::Type::Is;
    case Token::Type::As:
        return Precedence::Type::As;
    case Token::Type::Plus:
    case Token::Type::Minus:
        return Precedence::Type::Additive;
    case Token::Type::Asterisk:
    case Token::Type::Divide:
    case Token::Type::Modulo:
        return Precedence::Type::Multiplicative;
    case Token::Type::LeftParen:
    case Token::Type::LeftBracket:
    case Token::Type::Dot:
        return Precedence::Type::Postfix;
    default:
        return Precedence::Type::None;
    }
}
