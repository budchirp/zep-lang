module;

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module zep.lsp.protocol.types;

import zep.common.source.position;
import zep.common.source.span;

export namespace lsp {

class Position {
  private:
  public:
    std::uint32_t line;
    std::uint32_t character;

    Position() : line(0), character(0) {}

    Position(std::uint32_t line, std::uint32_t character) : line(line), character(character) {}

    bool operator==(const Position& other) const {
        return line == other.line && character == other.character;
    }
};

class Range {
  private:
  public:
    Position start;
    Position end;

    Range() = default;

    Range(Position start, Position end) : start(start), end(end) {}

    bool operator==(const Range& other) const { return start == other.start && end == other.end; }
};

class TextDocumentIdentifier {
  private:
  public:
    std::string uri;

    explicit TextDocumentIdentifier(std::string uri) : uri(std::move(uri)) {}
};

class VersionedTextDocumentIdentifier {
  private:
  public:
    std::string uri;
    std::int32_t version;

    VersionedTextDocumentIdentifier(std::string uri, std::int32_t version)
        : uri(std::move(uri)), version(version) {}
};

class TextDocumentItem {
  private:
  public:
    std::string uri;
    std::string language_id;
    std::int32_t version;
    std::string text;

    TextDocumentItem(std::string uri, std::string language_id, std::int32_t version,
                     std::string text)
        : uri(std::move(uri)), language_id(std::move(language_id)), version(version),
          text(std::move(text)) {}
};

class DiagnosticSeverity {
  public:
    enum class Type : std::uint8_t {
        Error = 1,
        Warning = 2,
        Information = 3,
        Hint = 4,
    };
};

class Diagnostic {
  private:
  public:
    Range range;
    DiagnosticSeverity::Type severity;
    std::string message;
    std::string source;
    std::string code;

    Diagnostic(Range range, DiagnosticSeverity::Type severity, std::string message,
               std::string source = "zep", std::string code = "")
        : range(range), severity(severity), message(std::move(message)), source(std::move(source)),
          code(std::move(code)) {}

    bool operator==(const Diagnostic& other) const {
        return range == other.range && severity == other.severity && message == other.message &&
               source == other.source && code == other.code;
    }
};

class CompletionItemKind {
  public:
    enum class Type : std::uint8_t {
        Text = 1,
        Method = 2,
        Function = 3,
        Constructor = 4,
        Field = 5,
        Variable = 6,
        Class = 7,
        Interface = 8,
        Module = 9,
        Property = 10,
        Unit = 11,
        Value = 12,
        Enum = 13,
        Keyword = 14,
        Snippet = 15,
        Color = 16,
        File = 17,
        Reference = 18,
        Folder = 19,
        EnumMember = 20,
        Constant = 21,
        Struct = 22,
        Event = 23,
        Operator = 24,
        TypeParameter = 25,
    };
};

class CompletionItem {
  private:
  public:
    std::string label;
    CompletionItemKind::Type kind;
    std::string detail;
    std::string documentation;

    CompletionItem(std::string label, CompletionItemKind::Type kind, std::string detail = "",
                   std::string documentation = "")
        : label(std::move(label)), kind(kind), detail(std::move(detail)),
          documentation(std::move(documentation)) {}
};

class Hover {
  private:
  public:
    std::string contents;
    std::optional<Range> range;

    explicit Hover(std::string contents, std::optional<Range> range = std::nullopt)
        : contents(std::move(contents)), range(range) {}
};

inline const std::vector<std::string> semantic_token_types = {
    "type",      "class",    "enum",     "interface",  "struct",   "typeParameter",
    "parameter", "variable", "property", "enumMember", "function", "method",
    "keyword",   "modifier", "comment",  "string",     "number",   "operator"};

inline const std::vector<std::string> semantic_token_modifiers = {
    "declaration", "definition", "readonly", "static", "defaultLibrary"};

class SemanticTokenType {
  public:
    enum class Index : std::uint32_t {
        Type = 0,
        Class = 1,
        Enum = 2,
        Interface = 3,
        Struct = 4,
        TypeParameter = 5,
        Parameter = 6,
        Variable = 7,
        Property = 8,
        EnumMember = 9,
        Function = 10,
        Method = 11,
        Keyword = 12,
        Modifier = 13,
        Comment = 14,
        String = 15,
        Number = 16,
        Operator = 17,
    };
};

class SemanticTokenModifier {
  public:
    static constexpr std::uint32_t None = 0;
    static constexpr std::uint32_t Declaration = 1U << 0U;
    static constexpr std::uint32_t Definition = 1U << 1U;
    static constexpr std::uint32_t Readonly = 1U << 2U;
    static constexpr std::uint32_t Static = 1U << 3U;
    static constexpr std::uint32_t DefaultLibrary = 1U << 4U;
};

class SemanticTokens {
  private:
  public:
    std::vector<std::uint32_t> data;

    explicit SemanticTokens(std::vector<std::uint32_t> data = {}) : data(std::move(data)) {}
};

class PositionConverter {
  private:
  public:
    static Position from_compiler(const ::Position& position) {
        auto line = position.line > 0 ? static_cast<std::uint32_t>(position.line - 1) : 0U;
        auto character = position.column > 0 ? static_cast<std::uint32_t>(position.column - 1) : 0U;

        return Position(line, character);
    }

    static Range from_compiler_span(const Span& span) {
        return Range(from_compiler(span.start), from_compiler(span.end));
    }

    static ::Position to_compiler(const Position& position) {
        auto line = static_cast<std::size_t>(position.line + 1);
        auto column = static_cast<std::size_t>(position.character + 1);

        return ::Position(line, column);
    }
};

} // namespace lsp
