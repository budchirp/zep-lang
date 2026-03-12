module;

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

export module zep.common.logger;

import zep.common.position;
import zep.common.source;
import zep.common.logger.colors;

export void print_indent(int depth) {
    for (int i = 0; i < depth; ++i) {
        std::cout << "  ";
    }
}

export class Logger {
  private:
    std::optional<Source> source;

    void print_prefix(std::string_view color, std::string_view level) const {
        std::cerr << color << colors::bold << level << colors::reset;
    }

    void print_location(Position position) const {
        std::cerr << colors::bold_white << source->name << ":" << position.line << ":"
                  << position.column << ": " << colors::reset;
    }

    void print_source_line(Position position) const {
        if (!source.has_value()) {
            return;
        }

        auto content = source->content;
        std::size_t current_line = 1;
        std::size_t line_start = 0;

        for (std::size_t i = 0; i < content.size(); ++i) {
            if (current_line == position.line) {
                line_start = i;
                break;
            }
            if (content[i] == '\n') {
                ++current_line;
            }
        }

        std::size_t line_end = content.find('\n', line_start);
        if (line_end == std::string_view::npos) {
            line_end = content.size();
        }

        auto source_line = content.substr(line_start, line_end - line_start);

        auto line_number_str = std::to_string(position.line);
        auto padding = std::string(line_number_str.size() + 1, ' ');

        std::cerr << colors::blue << padding << "|" << colors::reset << "\n";
        std::cerr << colors::blue << " " << line_number_str << " | " << colors::reset << source_line
                  << "\n";
        std::cerr << colors::blue << padding << "| " << colors::reset;

        if (position.column > 0) {
            std::cerr << std::string(position.column - 1, ' ');
        }
        std::cerr << colors::bold_red << "^" << colors::reset << "\n";
    }

  public:
    Logger() = default;

    explicit Logger(Source source) : source(std::move(source)) {}

    void log(std::string_view message) const {
        std::cerr << colors::bold_blue << "info: " << colors::reset << colors::bold_white << message
                  << colors::reset << "\n";
    }

    void warn(std::string_view message) const {
        std::cerr << colors::bold_yellow << "warning: " << colors::reset << colors::bold_white
                  << message << colors::reset << "\n";
    }

    [[noreturn]] void error(std::string_view message) const {
        std::cerr << colors::bold_red << "error: " << colors::reset << colors::bold_white << message
                  << colors::reset << "\n";
        std::exit(1);
    }

    void log(Position position, std::string_view message) const {
        print_location(position);
        print_prefix(colors::bold_blue, "info: ");
        std::cerr << colors::bold_white << message << colors::reset << "\n";
        print_source_line(position);
    }

    void warn(Position position, std::string_view message) const {
        print_location(position);
        print_prefix(colors::bold_yellow, "warning: ");
        std::cerr << colors::bold_white << message << colors::reset << "\n";
        print_source_line(position);
    }

    [[noreturn]] void error(Position position, std::string_view message) const {
        print_location(position);
        print_prefix(colors::bold_red, "error: ");
        std::cerr << colors::bold_white << message << colors::reset << "\n";
        print_source_line(position);
        std::exit(1);
    }
};
