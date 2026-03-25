module;

#include <string_view>
#include <unordered_map>

export module zep.frontend.token.keywords;

import zep.frontend.token;

export const std::unordered_map<std::string_view, Token::Type> keywords = {
    {"const", Token::Type::Const},     {"import", Token::Type::Import},
    {"struct", Token::Type::Struct},   {"fn", Token::Type::Fn},
    {"private", Token::Type::Private}, {"public", Token::Type::Public},
    {"return", Token::Type::Return},   {"var", Token::Type::Var},
    {"if", Token::Type::If},           {"else", Token::Type::Else},
    {"for", Token::Type::For},         {"mut", Token::Type::Mut},
    {"while", Token::Type::While},     {"true", Token::Type::Boolean},
    {"false", Token::Type::Boolean},   {"is", Token::Type::Is},
    {"extern", Token::Type::Extern},   {"as", Token::Type::As}};
