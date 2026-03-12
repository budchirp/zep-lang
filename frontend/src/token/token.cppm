module;

#include <string_view>

export module zep.frontend.token;

import zep.frontend.token.type;
import zep.common.position;

export class Token {
  public:
    TokenType type;
    Position position;
    std::string_view value;

    Token(TokenType type, Position position, std::string_view value)
        : type(type), position(position), value(value) {}
};
