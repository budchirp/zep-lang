module;

#include <cctype>
#include <cstddef>
#include <list>
#include <string>
#include <string_view>

export module zep.frontend.lexer;

import zep.frontend.token;
import zep.frontend.token.type;
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
        while (std::isalpha(static_cast<unsigned char>(ch)) ||
               std::isdigit(static_cast<unsigned char>(ch)) || ch == '_') {
            read_char();
        }

        return input.substr(start, byte_position - start);
    }

    TokenType read_number() {
        auto is_float = false;

        while (std::isdigit(static_cast<unsigned char>(ch))) {
            read_char();
        }

        if (ch == '.' && std::isdigit(static_cast<unsigned char>(peek_char()))) {
            is_float = true;
            read_char();
            while (std::isdigit(static_cast<unsigned char>(ch))) {
                read_char();
            }
        }

        return is_float ? TokenType::Float : TokenType::Number;
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
    explicit Lexer(const Source& source) : input(source.content) { read_char(); }

    Token next_token() {
        skip_whitespace();

        auto token_position = position;

        auto token_type = TokenType{};
        std::string_view value;

        switch (ch) {
        case '=':
            if (peek_char() == '=') {
                value = "==";
                token_type = TokenType::Equals;
                read_char();
                read_char();
            } else {
                value = "=";
                token_type = TokenType::Assign;
                read_char();
            }
            break;
        case '+':
            value = "+";
            token_type = TokenType::Plus;
            read_char();
            break;
        case '-':
            value = "-";
            token_type = TokenType::Minus;
            read_char();
            break;
        case '*':
            value = "*";
            token_type = TokenType::Asterisk;
            read_char();
            break;
        case '/':
            if (peek_char() == '/') {
                skip_comment();
                return next_token();
            }
            value = "/";
            token_type = TokenType::Divide;
            read_char();
            break;
        case '%':
            value = "%";
            token_type = TokenType::Modulo;
            read_char();
            break;
        case '<':
            if (peek_char() == '=') {
                value = "<=";
                token_type = TokenType::LessEqual;
                read_char();
                read_char();
            } else {
                value = "<";
                token_type = TokenType::LessThan;
                read_char();
            }
            break;
        case '>':
            if (peek_char() == '=') {
                value = ">=";
                token_type = TokenType::GreaterEqual;
                read_char();
                read_char();
            } else {
                value = ">";
                token_type = TokenType::GreaterThan;
                read_char();
            }
            break;
        case '!':
            if (peek_char() == '=') {
                value = "!=";
                token_type = TokenType::NotEquals;
                read_char();
                read_char();
            } else {
                value = "!";
                token_type = TokenType::Not;
                read_char();
            }
            break;
        case '&':
            if (peek_char() == '&') {
                value = "&&";
                token_type = TokenType::And;
                read_char();
                read_char();
            } else {
                value = "&";
                token_type = TokenType::Ampersand;
                read_char();
            }
            break;
        case '|':
            if (peek_char() == '|') {
                value = "||";
                token_type = TokenType::Or;
                read_char();
                read_char();
            } else {
                value = "|";
                token_type = TokenType::Illegal;
                read_char();
            }
            break;
        case '{':
            value = "{";
            token_type = TokenType::LeftBrace;
            read_char();
            break;
        case '}':
            value = "}";
            token_type = TokenType::RightBrace;
            read_char();
            break;
        case '(':
            value = "(";
            token_type = TokenType::LeftParen;
            read_char();
            break;
        case ')':
            value = ")";
            token_type = TokenType::RightParen;
            read_char();
            break;
        case '[':
            value = "[";
            token_type = TokenType::LeftBracket;
            read_char();
            break;
        case ']':
            value = "]";
            token_type = TokenType::RightBracket;
            read_char();
            break;
        case '.':
            if (peek_char() == '.' && byte_position + 2 < input.size() &&
                input[byte_position + 2] == '.') {
                value = "...";
                token_type = TokenType::Ellipsis;
                read_char();
                read_char();
                read_char();
            } else {
                value = ".";
                token_type = TokenType::Dot;
                read_char();
            }
            break;
        case ':':
            value = ":";
            token_type = TokenType::Colon;
            read_char();
            break;
        case ',':
            value = ",";
            token_type = TokenType::Comma;
            read_char();
            break;
        case ';':
            value = ";";
            token_type = TokenType::Semicolon;
            read_char();
            break;
        case '"':
            value = read_string();
            token_type = TokenType::String;
            break;
        case '@':
            value = "@";
            token_type = TokenType::At;
            read_char();
            break;
        case '\0':
            value = "";
            token_type = TokenType::Eof;
            break;
        default:
            if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
                value = read_identifier();
                auto it = keywords.find(value);
                token_type = (it != keywords.end()) ? it->second : TokenType::Identifier;
            } else if (std::isdigit(static_cast<unsigned char>(ch))) {
                auto start = byte_position;
                token_type = read_number();
                value = input.substr(start, byte_position - start);
            } else {
                value = "";
                token_type = TokenType::Illegal;
                read_char();
            }
            break;
        }

        return Token(token_type, token_position, value);
    }
};
