module;

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module zep.frontend.kind;

import zep.frontend.token;
import zep.common.source.span;

export class AssemblyOperand {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t { Input, Output, InputOutput };
    };

    Kind::Type kind;
    std::string name;
    std::string constraint;
    std::size_t argument;
    Span span;

    AssemblyOperand(Kind::Type kind, std::string name, std::string constraint, std::size_t argument,
                    Span span)
        : kind(kind), name(std::move(name)), constraint(std::move(constraint)), argument(argument),
          span(span) {}
};

export class InlineAssembly {
  public:
    std::vector<AssemblyOperand> operands;
    std::vector<std::string> clobbers;
    std::vector<std::string> options;
    std::string text;
    std::string constraints;
    bool pure;
    bool intel;

    InlineAssembly() : operands(), clobbers(), options(), text(), constraints(), pure(false), intel(false) {}
};

export class Linkage {
  public:
    enum class Type : std::uint8_t { External, Internal, LinkOnceODR };
};

export class Abi {
  public:
    enum class Type : std::uint8_t { Language, C };
};

export class Visibility {
  public:
    enum class Type : std::uint8_t { Public, Private };

    static std::string to_string(Type visibility) {
        return visibility == Type::Public ? "public" : "private";
    }
};

export class StorageKind {
  public:
    enum class Type : std::uint8_t { Var, VarMut };

    static std::string to_string(Type kind) {
        switch (kind) {
        case Type::Var:
            return "var";
        case Type::VarMut:
            return "var mut";
        }

        return "unknown";
    }
};

export class BinaryOperator {
  public:
    enum class Type : std::uint8_t {
        Plus,
        Minus,
        Asterisk,
        Divide,
        Modulo,
        Equals,
        NotEquals,
        LessThan,
        GreaterThan,
        LessEqual,
        GreaterEqual,
        And,
        Or,
        As,
        Is,
    };

    static std::string to_string(Type operation) {
        switch (operation) {
        case Type::Plus: return "+";
        case Type::Minus: return "-";
        case Type::Asterisk: return "*";
        case Type::Divide: return "/";
        case Type::Modulo: return "%";
        case Type::Equals: return "==";
        case Type::NotEquals: return "!=";
        case Type::LessThan: return "<";
        case Type::GreaterThan: return ">";
        case Type::LessEqual: return "<=";
        case Type::GreaterEqual: return ">=";
        case Type::And: return "&&";
        case Type::Or: return "||";
        case Type::As: return "as";
        case Type::Is: return "is";
        }

        return "?";
    }

    static std::optional<Type> from_token(Token::Type token) {
        switch (token) {
        case Token::Type::Plus: return Type::Plus;
        case Token::Type::Minus: return Type::Minus;
        case Token::Type::Asterisk: return Type::Asterisk;
        case Token::Type::Divide: return Type::Divide;
        case Token::Type::Modulo: return Type::Modulo;
        case Token::Type::Equals: return Type::Equals;
        case Token::Type::NotEquals: return Type::NotEquals;
        case Token::Type::LessThan: return Type::LessThan;
        case Token::Type::GreaterThan: return Type::GreaterThan;
        case Token::Type::LessEqual: return Type::LessEqual;
        case Token::Type::GreaterEqual: return Type::GreaterEqual;
        case Token::Type::And: return Type::And;
        case Token::Type::Or: return Type::Or;
        case Token::Type::As: return Type::As;
        case Token::Type::Is: return Type::Is;
        default: return std::nullopt;
        }
    }
};

export class UnaryOperator {
  public:
    enum class Type : std::uint8_t {
        Plus,
        Minus,
        Not,
        Dereference,
        AddressOf,
        AddressOfMut,
    };

    static std::string to_string(Type operation) {
        switch (operation) {
        case Type::Plus: return "+";
        case Type::Minus: return "-";
        case Type::Not: return "!";
        case Type::Dereference: return "*";
        case Type::AddressOf: return "&";
        case Type::AddressOfMut: return "&mut ";
        }

        return "?";
    }

    static std::optional<Type> from_token(Token::Type token) {
        switch (token) {
        case Token::Type::Plus: return Type::Plus;
        case Token::Type::Minus: return Type::Minus;
        case Token::Type::Not: return Type::Not;
        case Token::Type::Asterisk: return Type::Dereference;
        case Token::Type::Ampersand: return Type::AddressOf;
        default: return std::nullopt;
        }
    }
};

export class Coercion {
  public:
    enum class Type : std::uint8_t { None, BaseSlice, InterfaceValue, CopyValue, MoveValue };
};
