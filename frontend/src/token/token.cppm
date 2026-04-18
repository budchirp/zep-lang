module;

#include <cstdint>
#include <string_view>

export module zep.frontend.token;

import zep.common.position;
import zep.common.span;

export class Token {
  public:
    enum class Type : std::uint8_t {
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

    Type type;
    Span span;
    std::string_view value;

    Token(Type type, Span span, std::string_view value)
        : type(type), span(span), value(value) {}
};
