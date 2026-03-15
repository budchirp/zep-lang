module;

#include <cstdint>
#include <string>
#include <vector>

export module zep.checker.diagnostic;

import zep.common.position;
import zep.common.logger;

export enum class DiagnosticSeverity : std::uint8_t { Error, Warning };

export class Diagnostic {
  private:
  public:
    Position position;

    DiagnosticSeverity severity;

    std::string message;

    Diagnostic(Position position, DiagnosticSeverity severity, std::string message)
        : position(position), severity(severity), message(std::move(message)) {}

    void print(const Logger& logger) const {
        switch (severity) {
        case DiagnosticSeverity::Error:
            logger.report_error(position, message);
            break;
        case DiagnosticSeverity::Warning:
            logger.report_warning(position, message);
            break;
        }
    }
};

export class DiagnosticList {
  private:
    std::vector<Diagnostic> diagnostics;

  public:
    DiagnosticList() = default;

    void add_error(Position position, std::string message) {
        diagnostics.emplace_back(position, DiagnosticSeverity::Error, std::move(message));
    }

    void add_warning(Position position, std::string message) {
        diagnostics.emplace_back(position, DiagnosticSeverity::Warning, std::move(message));
    }

    bool has_errors() const {
        for (const auto& diagnostic : diagnostics) {
            if (diagnostic.severity == DiagnosticSeverity::Error) {
                return true;
            }
        }
        return false;
    }

    void print(const Logger& logger) const {
        for (const auto& diagnostic : diagnostics) {
            diagnostic.print(logger);
        }
    }
};
