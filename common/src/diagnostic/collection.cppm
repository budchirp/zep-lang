module;

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

export module zep.common.diagnostic.collection;

import zep.common.diagnostic.diagnostic;
import zep.common.source;
import zep.common.source.location;
import zep.common.source.span;

export class Diagnostics {
  private:
    const Source* default_source;
    std::unordered_set<std::string> seen;

    std::string key(const Source* source, Span span, DiagnosticSeverity::Type severity,
                    const std::string& message) const {
        return std::to_string(reinterpret_cast<std::uintptr_t>(source)) + ":" +
               std::to_string(span.start.line) + ":" + std::to_string(span.start.column) + ":" +
               std::to_string(span.end.line) + ":" + std::to_string(span.end.column) + ":" +
               std::to_string(static_cast<int>(severity)) + ":" + message;
    }

    void add(const Source* source, Span span, DiagnosticSeverity::Type severity,
             std::string message, std::string code = "") {
        if (!seen.insert(key(source, span, severity, message)).second) {
            return;
        }

        entries.emplace_back(SourceLocation(source, span), severity, std::move(message),
                             std::move(code));
    }

  public:
    std::vector<Diagnostic> entries;

    explicit Diagnostics(const Source* default_source = nullptr) : default_source(default_source) {}

    void add_error(Span span, std::string message, std::string code = "") {
        add(default_source, span, DiagnosticSeverity::Type::Error, std::move(message),
            std::move(code));
    }

    void add_error(const Source& source, Span span, std::string message, std::string code = "") {
        add(&source, span, DiagnosticSeverity::Type::Error, std::move(message), std::move(code));
    }

    void add_warning(Span span, std::string message, std::string code = "") {
        add(default_source, span, DiagnosticSeverity::Type::Warning, std::move(message),
            std::move(code));
    }

    void add_warning(const Source& source, Span span, std::string message, std::string code = "") {
        add(&source, span, DiagnosticSeverity::Type::Warning, std::move(message), std::move(code));
    }

    bool has_errors() const {
        return std::ranges::any_of(entries, [](const Diagnostic& diagnostic) {
            return diagnostic.severity == DiagnosticSeverity::Type::Error;
        });
    }

    bool has_warnings() const {
        return std::ranges::any_of(entries, [](const Diagnostic& diagnostic) {
            return diagnostic.severity == DiagnosticSeverity::Type::Warning;
        });
    }

    std::size_t error_count() const {
        return static_cast<std::size_t>(std::ranges::count_if(entries, [](const Diagnostic& d) {
            return d.severity == DiagnosticSeverity::Type::Error;
        }));
    }

    std::size_t warning_count() const {
        return static_cast<std::size_t>(std::ranges::count_if(entries, [](const Diagnostic& d) {
            return d.severity == DiagnosticSeverity::Type::Warning;
        }));
    }

    const std::vector<Diagnostic>& all() const { return entries; }

    void print() const {
        for (const auto& diagnostic : entries) {
            diagnostic.print();
        }
    }
};
