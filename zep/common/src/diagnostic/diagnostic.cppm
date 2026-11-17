module;

#include <cstdint>
#include <string>

export module zep.common.diagnostic.diagnostic;

import zep.common.source.position;
import zep.common.source.span;
import zep.common.logger;
import zep.common.source.location;

export class DiagnosticSeverity {
  public:
    enum class Type : std::uint8_t { Error, Warning };
};

export class Diagnostic {
  public:
    SourceLocation location;
    DiagnosticSeverity::Type severity;

    std::string message;
    std::string code;

    Diagnostic(SourceLocation location, DiagnosticSeverity::Type severity, std::string message,
               std::string code = "")
        : location(location), severity(severity), message(std::move(message)),
          code(std::move(code)) {}

    void print() const {
        if (location.source == nullptr) {
            return;
        }

        Logger logger(*location.source);
        if (severity == DiagnosticSeverity::Type::Error) {
            logger.report_error(location.span, message);
        } else {
            logger.report_warning(location.span, message);
        }
    }
};
