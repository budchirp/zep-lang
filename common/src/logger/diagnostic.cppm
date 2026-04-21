module;

#include <cstdint>
#include <string>
#include <vector>

export module zep.common.logger.diagnostic;

import zep.common.position;
import zep.common.span;
import zep.common.logger;

export class DiagnosticSeverity {
  private:

  public:
    enum class Type : std::uint8_t { Error, Warning };
};

export class Diagnostic {
  private:
  public:
    Span span;

    DiagnosticSeverity::Type severity;

    std::string message;

    Diagnostic(Span span, DiagnosticSeverity::Type severity, std::string message)
        : span(span), severity(severity), message(std::move(message)) {}

    void print(const Logger& logger) const {
        switch (severity) {
        case DiagnosticSeverity::Type::Error:
            logger.report_error(span, message);
            break;
        case DiagnosticSeverity::Type::Warning:
            logger.report_warning(span, message);
            break;
        }
    }
};

export class DiagnosticList {
  private:
    std::vector<Diagnostic> diagnostics;

  public:
    DiagnosticList() = default;

    void add_error(Span span, std::string message) {
        diagnostics.emplace_back(span, DiagnosticSeverity::Type::Error, std::move(message));
    }

    void add_warning(Span span, std::string message) {
        diagnostics.emplace_back(span, DiagnosticSeverity::Type::Warning, std::move(message));
    }

    bool has_errors() const {
        for (const auto& diagnostic : diagnostics) {
            if (diagnostic.severity == DiagnosticSeverity::Type::Error) {
                return true;
            }
        }

        return false;
    }

    void print(const Logger& logger) const {
        bool first = true;
        for (const auto& diagnostic : diagnostics) {
            if (!first) {
                Logger::print("\n");
            }
            first = false;
            diagnostic.print(logger);
        }
    }
};
