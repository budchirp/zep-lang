module;

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

export module zep.lsp.protocol.transport;

import zep.common.logger;

export class Transport {
  private:
    std::istream& input;
    std::ostream& output;

    static std::string trim(std::string_view view) {
        while (!view.empty() && std::isspace(static_cast<unsigned char>(view.front()))) {
            view.remove_prefix(1);
        }

        while (!view.empty() && std::isspace(static_cast<unsigned char>(view.back()))) {
            view.remove_suffix(1);
        }

        return std::string(view);
    }

  public:
    Transport(std::istream& input, std::ostream& output) : input(input), output(output) {}

    std::optional<nlohmann::json> read_message() {
        std::size_t content_length = 0;
        bool has_length = false;

        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.empty()) {
                break;
            }

            constexpr std::string_view prefix = "Content-Length:";
            if (line.size() >= prefix.size() && line.compare(0, prefix.size(), prefix) == 0) {
                auto length_text = trim(std::string_view(line).substr(prefix.size()));
                try {
                    content_length = std::stoull(length_text);
                    has_length = true;
                } catch (...) {
                    return std::nullopt;
                }
            }
        }

        if (!has_length || content_length == 0) {
            return std::nullopt;
        }

        std::string body(content_length, '\0');
        input.read(body.data(), static_cast<std::streamsize>(content_length));
        if (input.gcount() != static_cast<std::streamsize>(content_length)) {
            return std::nullopt;
        }

        try {
            return nlohmann::json::parse(body);
        } catch (const nlohmann::json::parse_error&) {
            Logger::print_stderr("zep-lsp: transport: json parse error\n");
            return std::nullopt;
        }
    }

    void write_message(const nlohmann::json& payload) {
        auto body = payload.dump();
        output << "Content-Length: " << body.size() << "\r\n\r\n" << body;
        output.flush();
    }
};
