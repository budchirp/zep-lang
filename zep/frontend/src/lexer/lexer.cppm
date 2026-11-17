module;

#include <cctype>
#include <cstddef>
#include <list>
#include <string>
#include <string_view>

export module zep.frontend.lexer;

import zep.frontend.token;
import zep.frontend.token.keywords;
import zep.common.source.position;
import zep.common.source.span;
import zep.common.source;

export class LexerCheckpoint {
  public:
    Position position = Position(1, 0);

    std::size_t byte_position = 0;

    std::size_t read_position = 0;

    char ch = '\0';
};

export class Lexer {
  private:
    std::string_view input;

    LexerCheckpoint cursor;

    std::list<std::string> owned_strings;

    void read_char() {
        if (cursor.read_position >= input.size()) {
            cursor.ch = '\0';
        } else {
            cursor.ch = input[cursor.read_position];
        }

        cursor.byte_position = cursor.read_position;
        cursor.read_position++;

        if (cursor.ch == '\n') {
            cursor.position.next_line();
        } else {
            cursor.position.increment_column();
        }
    }

    [[nodiscard]] char peek_char() const {
        if (cursor.read_position >= input.size()) {
            return '\0';
        }

        return input[cursor.read_position];
    }

    void skip_whitespace() {
        while (cursor.ch == ' ' || cursor.ch == '\t' || cursor.ch == '\r' || cursor.ch == '\n') {
            read_char();
        }
    }

  public:
    [[nodiscard]] LexerCheckpoint take_checkpoint() const { return cursor; }

    void restore_checkpoint(const LexerCheckpoint& checkpoint) { cursor = checkpoint; }

    std::string_view read_identifier() {
        auto start = cursor.byte_position;
        while ((std::isalpha(static_cast<unsigned char>(cursor.ch)) != 0) ||
               (std::isdigit(static_cast<unsigned char>(cursor.ch)) != 0) || cursor.ch == '_') {
            read_char();
        }

        return input.substr(start, cursor.byte_position - start);
    }

    Token::Type read_number() {
        auto is_float = false;

        if (cursor.ch == '0') {
            auto next = peek_char();
            if (next == 'x' || next == 'X') {
                read_char();
                read_char();
                while (std::isxdigit(static_cast<unsigned char>(cursor.ch)) != 0) {
                    read_char();
                }

                return Token::Type::Number;
            }
        }

        while (std::isdigit(static_cast<unsigned char>(cursor.ch)) != 0) {
            read_char();
        }

        if (cursor.ch == '.' && (std::isdigit(static_cast<unsigned char>(peek_char())) != 0)) {
            is_float = true;
            read_char();
            while (std::isdigit(static_cast<unsigned char>(cursor.ch)) != 0) {
                read_char();
            }
        }

        return is_float ? Token::Type::Float : Token::Type::Number;
    }

    std::string_view read_string() {
        owned_strings.emplace_back();
        auto& string = owned_strings.back();

        read_char();
        while (cursor.ch != '"' && cursor.ch != '\0') {
            if (cursor.ch == '\\') {
                read_char();
                switch (cursor.ch) {
                case 'n':
                    string += '\n';
                    break;
                case 'r':
                    string += '\r';
                    break;
                case 't':
                    string += '\t';
                    break;
                case '\\':
                    string += '\\';
                    break;
                case '"':
                    string += '"';
                    break;
                default:
                    string += '\\';
                    string += cursor.ch;
                    break;
                }
            } else {
                string += cursor.ch;
            }
            read_char();
        }

        if (cursor.ch == '"') {
            read_char();
        }

        return string;
    }

    std::string_view read_char_literal() {
        owned_strings.emplace_back();
        auto& string = owned_strings.back();

        read_char();
        if (cursor.ch == '\\') {
            read_char();
            switch (cursor.ch) {
            case '0':
                string += '\0';
                break;
            case 'n':
                string += '\n';
                break;
            case 'r':
                string += '\r';
                break;
            case 't':
                string += '\t';
                break;
            case '\\':
                string += '\\';
                break;
            case '\'':
                string += '\'';
                break;
            default:
                string += cursor.ch;
                break;
            }
            read_char();
        } else if (cursor.ch != '\'' && cursor.ch != '\0') {
            string += cursor.ch;
            read_char();
        }

        if (cursor.ch == '\'') {
            read_char();
        }

        return string;
    }

    void skip_comment() {
        while (cursor.ch != '\n' && cursor.ch != '\0') {
            read_char();
        }
    }

  public:
    explicit Lexer(std::string_view input) : input(input) { read_char(); }

    Token next_token() {
        while (true) {
            skip_whitespace();

            if (cursor.ch == '/' && peek_char() == '/') {
                skip_comment();
                continue;
            }

            break;
        }

        auto start_position = cursor.position;

        Token::Type token_type{};
        std::string_view value;

        switch (cursor.ch) {
        case '=':
            if (peek_char() == '=') {
                value = "==";
                token_type = Token::Type::Equals;
                read_char();
                read_char();
            } else {
                value = "=";
                token_type = Token::Type::Assign;
                read_char();
            }
            break;
        case '+':
            value = "+";
            token_type = Token::Type::Plus;
            read_char();
            break;
        case '-':
            if (peek_char() == '>') {
                value = "->";
                token_type = Token::Type::Arrow;
                read_char();
                read_char();
            } else {
                value = "-";
                token_type = Token::Type::Minus;
                read_char();
            }
            break;
        case '*':
            value = "*";
            token_type = Token::Type::Asterisk;
            read_char();
            break;
        case '/':
            value = "/";
            token_type = Token::Type::Divide;
            read_char();
            break;
        case '%':
            value = "%";
            token_type = Token::Type::Modulo;
            read_char();
            break;
        case '<':
            if (peek_char() == '=') {
                value = "<=";
                token_type = Token::Type::LessEqual;
                read_char();
                read_char();
            } else {
                value = "<";
                token_type = Token::Type::LessThan;
                read_char();
            }
            break;
        case '>':
            if (peek_char() == '=') {
                value = ">=";
                token_type = Token::Type::GreaterEqual;
                read_char();
                read_char();
            } else {
                value = ">";
                token_type = Token::Type::GreaterThan;
                read_char();
            }
            break;
        case '!':
            if (peek_char() == '=') {
                value = "!=";
                token_type = Token::Type::NotEquals;
                read_char();
                read_char();
            } else {
                value = "!";
                token_type = Token::Type::Not;
                read_char();
            }
            break;
        case '&':
            if (peek_char() == '&') {
                value = "&&";
                token_type = Token::Type::And;
                read_char();
                read_char();
            } else {
                value = "&";
                token_type = Token::Type::Ampersand;
                read_char();
            }
            break;
        case '~':
            value = "~";
            token_type = Token::Type::Tilde;
            read_char();
            break;
        case '|':
            if (peek_char() == '|') {
                value = "||";
                token_type = Token::Type::Or;
                read_char();
                read_char();
            } else {
                value = "|";
                token_type = Token::Type::Illegal;
                read_char();
            }
            break;
        case '{':
            value = "{";
            token_type = Token::Type::LeftBrace;
            read_char();
            break;
        case '}':
            value = "}";
            token_type = Token::Type::RightBrace;
            read_char();
            break;
        case '(':
            value = "(";
            token_type = Token::Type::LeftParen;
            read_char();
            break;
        case ')':
            value = ")";
            token_type = Token::Type::RightParen;
            read_char();
            break;
        case '[':
            value = "[";
            token_type = Token::Type::LeftBracket;
            read_char();
            break;
        case ']':
            value = "]";
            token_type = Token::Type::RightBracket;
            read_char();
            break;
        case '.':
            if (peek_char() == '.' && cursor.byte_position + 2 < input.size() &&
                input[cursor.byte_position + 2] == '.') {
                value = "...";
                token_type = Token::Type::Ellipsis;
                read_char();
                read_char();
                read_char();
            } else {
                value = ".";
                token_type = Token::Type::Dot;
                read_char();
            }
            break;
        case ':':
            if (peek_char() == ':') {
                value = "::";
                token_type = Token::Type::DoubleColon;
                read_char();
                read_char();
            } else {
                value = ":";
                token_type = Token::Type::Colon;
                read_char();
            }
            break;
        case ',':
            value = ",";
            token_type = Token::Type::Comma;
            read_char();
            break;
        case ';':
            value = ";";
            token_type = Token::Type::Semicolon;
            read_char();
            break;
        case '"':
            value = read_string();
            token_type = Token::Type::String;
            break;
        case '\'':
            value = read_char_literal();
            token_type = Token::Type::Char;
            break;
        case '@':
            value = "@";
            token_type = Token::Type::At;
            read_char();
            break;
        case '#':
            value = "#";
            token_type = Token::Type::Hash;
            read_char();
            break;
        case '\0':
            value = "";
            token_type = Token::Type::Eof;
            break;
        default:
            if ((std::isalpha(static_cast<unsigned char>(cursor.ch)) != 0) || cursor.ch == '_') {
                value = read_identifier();
                auto it = keywords.find(value);
                token_type = (it != keywords.end()) ? it->second : Token::Type::Identifier;
            } else if (std::isdigit(static_cast<unsigned char>(cursor.ch)) != 0) {
                auto start = cursor.byte_position;
                token_type = read_number();
                value = input.substr(start, cursor.byte_position - start);
            } else {
                value = "";
                token_type = Token::Type::Illegal;
                read_char();
            }
            break;
        }

        return Token(token_type, Span(start_position, cursor.position), value);
    }
};
