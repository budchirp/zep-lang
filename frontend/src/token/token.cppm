module;

#include <cstdint>
#include <string_view>

export module zep.frontend.token;

import zep.common.source.position;
import zep.common.source.span;

export class Token {
  public:
    enum class Type : std::uint8_t {
        Identifier,
        Number,
        Float,
        String,
        Char,
        Boolean,
        Null,

        Import,
        Interface,
        Struct,
        Enum,
        Fn,
        Return,
        Var,
        Const,
        If,
        Else,
        When,
        For,
        In,
        Mut,
        While,
        Static,
        Public,
        Private,
        Extern,
        Is,
        As,
        Type,
        Defer,
        Do,
        Override,

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
        Tilde,
        At,
        Hash,
        Not,
        Divide,
        Modulo,
        Arrow,

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
        DoubleColon,
        Colon,
        Comma,
        Semicolon,

        Eof,
        Illegal
    };

    Type type;
    Span span;
    std::string_view value;

    Token(Type type, Span span, std::string_view value) : type(type), span(span), value(value) {}
};
