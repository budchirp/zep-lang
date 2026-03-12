module;

#include <cstdint>

export module zep.frontend.token.type;

export enum class TokenType : std::uint8_t {
    Identifier,
    Number,
    Float,
    String,
    Boolean,

    Const,
    Import,
    Struct,
    Fn,
    Return,
    Var,
    If,
    Else,
    For,
    Mut,
    While,
    Public,
    Private,
    Extern,
    Is,
    As,

    Assign,
    Plus,
    Minus,
    Equals,
    NotEquals,
    LessEqual,
    GreaterEqual,
    LessThan,
    GreaterThan,
    Asterisk,
    Ampersand,
    At,
    Not,
    Divide,
    Modulo,

    And,
    Or,

    LeftBrace,
    RightBrace,
    LeftParen,
    RightParen,
    LeftBracket,
    RightBracket,

    Dot,
    Ellipsis,
    Colon,
    Comma,
    Semicolon,

    Eof,
    Illegal
};
