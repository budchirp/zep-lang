module;

#include <cstdint>
#include <string>
#include <vector>

export module zep.common.logger.diagnostic;

import zep.common.position;
import zep.common.logger;

export class DiagnosticSeverity {
  public:
    enum class Type : std::uint8_t { Error, Warning };
};

export class Diagnostic {
  private:
  public:
    Position position;

    DiagnosticSeverity::Type severity;

    std::string message;

    Diagnostic(Position position, DiagnosticSeverity::Type severity, std::string message)
        : position(position), severity(severity), message(std::move(message)) {}

    void print(const Logger& logger) const {
        switch (severity) {
        case DiagnosticSeverity::Type::Error:
            logger.report_error(position, message);
            break;
        case DiagnosticSeverity::Type::Warning:
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
        diagnostics.emplace_back(position, DiagnosticSeverity::Type::Error, std::move(message));
    }

    void add_warning(Position position, std::string message) {
        diagnostics.emplace_back(position, DiagnosticSeverity::Type::Warning, std::move(message));
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
