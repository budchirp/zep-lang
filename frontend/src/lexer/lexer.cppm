module;

#include <cctype>
#include <cstddef>
#include <list>
#include <string>
#include <string_view>

export module zep.frontend.lexer;

import zep.frontend.token;
import zep.frontend.token.keywords;
import zep.common.position;
import zep.common.source;

export class Lexer {
  private:
    std::string_view input;

    Position position = Position(1, 0);

    std::size_t byte_position = 0;
    std::size_t read_position = 0;

    char ch = '\0';

    std::list<std::string> owned_strings;

    void read_char() {
        if (read_position >= input.size()) {
            ch = '\0';
        } else {
            ch = input[read_position];
        }

        byte_position = read_position;
        read_position++;

        if (ch == '\n') {
            position.next_line();
        } else {
            position.increment_column();
        }
    }

    [[nodiscard]] char peek_char() const {
        if (read_position >= input.size()) {
            return '\0';
        }

        return input[read_position];
    }

    void skip_whitespace() {
        while (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            read_char();
        }
    }

    std::string_view read_identifier() {
        auto start = byte_position;
        while ((std::isalpha(static_cast<unsigned char>(ch)) != 0) ||
               (std::isdigit(static_cast<unsigned char>(ch)) != 0) || ch == '_') {
            read_char();
        }

        return input.substr(start, byte_position - start);
    }

    Token::Type read_number() {
        auto is_float = false;

        while (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            read_char();
        }

        if (ch == '.' && (std::isdigit(static_cast<unsigned char>(peek_char())) != 0)) {
            is_float = true;
            read_char();
            while (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
                read_char();
            }
        }

        return is_float ? Token::Type::Float : Token::Type::Number;
    }

    std::string_view read_string() {
        owned_strings.emplace_back();
        auto& string = owned_strings.back();

        read_char();
        while (ch != '"' && ch != '\0') {
            if (ch == '\\') {
                read_char();
                switch (ch) {
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
                    string += ch;
                    break;
                }
            } else {
                string += ch;
            }
            read_char();
        }

        if (ch == '"') {
            read_char();
        }

        return string;
    }

    void skip_comment() {
        while (ch != '\n' && ch != '\0') {
            read_char();
        }
    }

  public:
    explicit Lexer(std::string_view input) : input(input) { read_char(); }

    Token next_token() {
        skip_whitespace();

        auto token_position = position;

        Token::Type token_type{};
        std::string_view value;

        switch (ch) {
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
            value = "-";
            token_type = Token::Type::Minus;
            read_char();
            break;
        case '*':
            value = "*";
            token_type = Token::Type::Asterisk;
            read_char();
            break;
        case '/':
            if (peek_char() == '/') {
                skip_comment();
                return next_token();
            }
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
            if (peek_char() == '.' && byte_position + 2 < input.size() &&
                input[byte_position + 2] == '.') {
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
            value = ":";
            token_type = Token::Type::Colon;
            read_char();
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
        case '@':
            value = "@";
            token_type = Token::Type::At;
            read_char();
            break;
        case '\0':
            value = "";
            token_type = Token::Type::Eof;
            break;
        default:
            if ((std::isalpha(static_cast<unsigned char>(ch)) != 0) || ch == '_') {
                value = read_identifier();
                auto it = keywords.find(value);
                token_type = (it != keywords.end()) ? it->second : Token::Type::Identifier;
            } else if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
                auto start = byte_position;
                token_type = read_number();
                value = input.substr(start, byte_position - start);
            } else {
                value = "";
                token_type = Token::Type::Illegal;
                read_char();
            }
            break;
        }

        return Token(token_type, token_position, value);
    }
};
