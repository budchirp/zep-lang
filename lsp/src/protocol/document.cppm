module;

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

export module zep.lsp.document;

import zep.common.source.position;
import zep.common.source.span;

export std::filesystem::path document_path(std::string_view uri) {
    if (uri.starts_with("file://")) {
        uri.remove_prefix(7);
    }

    std::string decoded;
    decoded.reserve(uri.length());
    for (std::size_t index = 0; index < uri.length(); ++index) {
        if (uri[index] != '%' || index + 2 >= uri.length()) {
            decoded.push_back(uri[index]);
            continue;
        }

        auto hexadecimal = [](char character) -> int {
            if (character >= '0' && character <= '9') {
                return character - '0';
            }
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            return character >= 'a' && character <= 'f' ? character - 'a' + 10 : -1;
        };
        auto high = hexadecimal(uri[index + 1]);
        auto low = hexadecimal(uri[index + 2]);
        if (high < 0 || low < 0) {
            decoded.push_back(uri[index]);
            continue;
        }
        decoded.push_back(static_cast<char>((high << 4) | low));
        index += 2;
    }
    return std::filesystem::path(decoded);
}

export std::string document_uri(const std::filesystem::path& path) {
    constexpr std::string_view hexadecimal = "0123456789ABCDEF";
    auto source = std::filesystem::absolute(path).lexically_normal().string();
    std::string encoded = "file://";
    encoded.reserve(source.length() + 7);
    for (const auto character : source) {
        auto value = static_cast<unsigned char>(character);
        if (std::isalnum(value) != 0 || character == '/' || character == '-' || character == '_' ||
            character == '.' || character == '~') {
            encoded.push_back(character);
            continue;
        }
        encoded.push_back('%');
        encoded.push_back(hexadecimal[value >> 4U]);
        encoded.push_back(hexadecimal[value & 0x0FU]);
    }
    return encoded;
}

export class Document {
  private:
    static std::size_t sequence_length(unsigned char value) {
        if ((value & 0x80U) == 0) {
            return 1;
        }
        if ((value & 0xE0U) == 0xC0U) {
            return 2;
        }
        if ((value & 0xF0U) == 0xE0U) {
            return 3;
        }
        return 4;
    }

    static std::uint32_t code_point(std::string_view content, std::size_t offset,
                                    std::size_t length) {
        auto first = static_cast<unsigned char>(content[offset]);
        if (length == 1) {
            return first;
        }
        std::uint32_t result = first & (0x7FU >> length);
        for (std::size_t index = 1; index < length && offset + index < content.length(); ++index) {
            result = (result << 6U) |
                     (static_cast<unsigned char>(content[offset + index]) & 0x3FU);
        }
        return result;
    }

    std::size_t offset(std::size_t requested_line, std::size_t requested_character) const {
        std::size_t line = 0;
        std::size_t index = 0;
        while (index < text.length() && line < requested_line) {
            if (text[index++] == '\n') {
                ++line;
            }
        }
        if (line != requested_line) {
            throw std::out_of_range("document position line is out of range");
        }

        std::size_t character = 0;
        while (index < text.length() && text[index] != '\n' && character < requested_character) {
            auto length = sequence_length(static_cast<unsigned char>(text[index]));
            auto units = code_point(text, index, length) > 0xFFFFU ? 2U : 1U;
            if (character + units > requested_character) {
                throw std::out_of_range("document position splits a UTF-16 character");
            }
            character += units;
            index += length;
        }
        if (character != requested_character) {
            throw std::out_of_range("document position character is out of range");
        }
        return index;
    }

  public:
    std::string uri;
    std::filesystem::path path;
    std::int64_t version;
    std::string text;

    Document(std::string uri, std::int64_t version, std::string text)
        : uri(std::move(uri)), path(document_path(this->uri)), version(version),
          text(std::move(text)) {}

    Position position(const nlohmann::json& value) const {
        auto byte_offset = offset(value.at("line").get<std::size_t>(),
                                  value.at("character").get<std::size_t>());
        std::size_t line = 1;
        std::size_t column = 1;
        for (std::size_t index = 0; index < byte_offset; ++index) {
            if (text[index] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
        return Position(line, column);
    }

    std::size_t utf16_character(Position position) const {
        auto line_start = offset(position.line - 1, 0);
        auto target = std::min(line_start + position.column - 1, text.length());
        std::size_t result = 0;
        for (auto index = line_start; index < target;) {
            auto length = sequence_length(static_cast<unsigned char>(text[index]));
            result += code_point(text, index, length) > 0xFFFFU ? 2U : 1U;
            index += length;
        }
        return result;
    }

    bool change(std::int64_t next_version, const nlohmann::json& changes) {
        if (next_version <= version) {
            return false;
        }
        if (!changes.is_array()) {
            throw std::invalid_argument("contentChanges must be an array");
        }

        for (const auto& change : changes) {
            auto replacement = change.at("text").get<std::string>();
            if (!change.contains("range")) {
                text = std::move(replacement);
                continue;
            }

            const auto& range = change.at("range");
            auto start = offset(range.at("start").at("line").get<std::size_t>(),
                                range.at("start").at("character").get<std::size_t>());
            auto end = offset(range.at("end").at("line").get<std::size_t>(),
                              range.at("end").at("character").get<std::size_t>());
            if (end < start) {
                throw std::invalid_argument("document change range is reversed");
            }
            text.replace(start, end - start, replacement);
        }

        version = next_version;
        return true;
    }
};
