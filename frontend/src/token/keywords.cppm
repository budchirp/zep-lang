module;

#include <string_view>
#include <unordered_map>

export module zep.frontend.token.keywords;

import zep.frontend.token;

export const std::unordered_map<std::string_view, Token::Type> keywords = {
    {"import", Token::Type::Import}, {"interface", Token::Type::Interface},
    {"struct", Token::Type::Struct}, {"enum", Token::Type::Enum},
    {"fn", Token::Type::Fn},         {"private", Token::Type::Private},
    {"public", Token::Type::Public}, {"return", Token::Type::Return},
    {"var", Token::Type::Var},       {"const", Token::Type::Const},
    {"if", Token::Type::If},         {"else", Token::Type::Else},
    {"when", Token::Type::When},     {"for", Token::Type::For},
    {"in", Token::Type::In},         {"mut", Token::Type::Mut},
    {"while", Token::Type::While},   {"true", Token::Type::Boolean},
    {"false", Token::Type::Boolean}, {"null", Token::Type::Null},
    {"is", Token::Type::Is},         {"extern", Token::Type::Extern},
    {"as", Token::Type::As},         {"type", Token::Type::Type},
    {"static", Token::Type::Static}, {"defer", Token::Type::Defer},
    {"do", Token::Type::Do},         {"override", Token::Type::Override}};
