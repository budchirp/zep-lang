module;

#include <cstdio>
#include <cstdlib>
#include <print>
#include <string>
#include <string_view>
#include <utility>

export module zep.common.logger;

import zep.common.position;
import zep.common.span;
import zep.common.source;
import zep.common.logger.colors;

export class Logger {
  private:
    const Source* source = nullptr;

    void print_location(Span span) const {
        if (source == nullptr) {
            print_stderr(colors::bold_white, "<no source>:", colors::reset, " ");
            return;
        }

        print_stderr(colors::bold_white, source->name, ":", span.start.line, ":", span.start.column,
                     ": ", colors::reset);
    }

    void print_source_line(Span span) const {
        if (source == nullptr) {
            return;
        }

        const auto& content = source->content;

        std::size_t current_line = 1;
        std::size_t line_start = 0;

        for (std::size_t i = 0; i < content.size(); ++i) {
            if (current_line == span.start.line) {
                line_start = i;
                break;
            }
            if (content[i] == '\n') {
                ++current_line;
            }
        }

        std::size_t line_end = content.find('\n', line_start);
        if (line_end == std::string::npos) {
            line_end = content.size();
        }

        auto source_line = content.substr(line_start, line_end - line_start);

        auto line_number_str = std::to_string(span.start.line);
        auto padding = std::string(line_number_str.size() + 2, ' ');

        print_stderr(colors::blue, padding, "|", colors::reset, "\n");
        print_stderr(colors::blue, " ", line_number_str, " | ", colors::reset, source_line, "\n");
        print_stderr(colors::blue, padding, "| ", colors::reset);

        if (span.start.column > 0) {
            print_stderr(std::string(span.start.column - 1, ' '));
        }

        std::size_t length = 1;
        if (span.start.line == span.end.line && span.end.column > span.start.column) {
            length = span.end.column - span.start.column;
        }

        print_stderr(colors::bold_red, std::string(length, '~'), colors::reset, "\n");
    }

    void emit_diagnostic(Span span, std::string_view level_color,
                         std::string_view level_label, std::string_view message) const {
        print_location(span);
        print_stderr(level_color, colors::bold, level_label, colors::reset);
        print_stderr(colors::bold_white, message, colors::reset, "\n");
        print_source_line(span);
    }

  public:
    Logger() = default;

    explicit Logger(const Source& source) : source(&source) {}

    static void print_indent(int depth) {
        for (int i = 0; i < depth; ++i) {
            std::print("  ");
        }
    }

    template <typename... Args>
    static void print(Args&&... args) {
        if constexpr (sizeof...(args) > 0) {
            (std::print("{}", std::forward<Args>(args)), ...);
        }
    }

    template <typename... Args>
    static void print_stderr(Args&&... args) {
        (std::print(stderr, "{}", std::forward<Args>(args)), ...);
    }

    void log(std::string_view message) const {
        print_stderr(colors::bold_blue, "info: ", colors::reset, colors::bold_white, message,
                     colors::reset, "\n");
    }

    void warn(std::string_view message) const {
        print_stderr(colors::bold_yellow, "warning: ", colors::reset, colors::bold_white, message,
                     colors::reset, "\n");
    }

    [[noreturn]] void error(std::string_view message) const {
        print_stderr(colors::bold_red, "error: ", colors::reset, colors::bold_white, message,
                     colors::reset, "\n");
        std::exit(1);
    }

    void log(Span span, std::string_view message) const {
        emit_diagnostic(span, colors::bold_blue, "info: ", message);
    }

    void warn(Span span, std::string_view message) const {
        emit_diagnostic(span, colors::bold_yellow, "warning: ", message);
    }

    [[noreturn]] void error(Span span, std::string_view message) const {
        emit_diagnostic(span, colors::bold_red, "error: ", message);
        std::exit(1);
    }

    void report_error(Span span, std::string_view message) const {
        emit_diagnostic(span, colors::bold_red, "error: ", message);
    }

    void report_warning(Span span, std::string_view message) const {
        emit_diagnostic(span, colors::bold_yellow, "warning: ", message);
    }
};
