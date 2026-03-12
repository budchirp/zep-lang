module;

#include <string_view>
#include <unordered_map>

export module zep.frontend.token.keywords;

import zep.frontend.token.type;

export const std::unordered_map<std::string_view, TokenType> keywords = {
    {"const", TokenType::Const},   {"import", TokenType::Import},   {"struct", TokenType::Struct},
    {"fn", TokenType::Fn},         {"private", TokenType::Private}, {"public", TokenType::Public},
    {"return", TokenType::Return}, {"var", TokenType::Var},         {"if", TokenType::If},
    {"else", TokenType::Else},     {"for", TokenType::For},         {"mut", TokenType::Mut},
    {"while", TokenType::While},   {"true", TokenType::Boolean},    {"false", TokenType::Boolean},
    {"is", TokenType::Is},         {"extern", TokenType::Extern},   {"as", TokenType::As}};
