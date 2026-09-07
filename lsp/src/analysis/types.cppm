module;

#include <cstdint>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module zep.lsp.analysis.types;

import zep.common.source.span;

export class AnalysisLocation {
  public:
    std::filesystem::path path;
    Span span;

    AnalysisLocation(std::filesystem::path path, Span span)
        : path(std::move(path)), span(span) {}
};

export class CompletionKind {
  public:
    enum class Type : std::uint8_t {
        Method,
        Function,
        Field,
        Variable,
        Interface,
        Module,
        Enum,
        Keyword,
        EnumMember,
        Constant,
        Struct,
        TypeParameter,
    };
};

export class Completion {
  public:
    std::string label;
    CompletionKind::Type kind;
    std::string detail;
    Span replacement;
    std::string insertion;
    std::string sort_key;

    Completion(std::string label, CompletionKind::Type kind, std::string detail = "",
               Span replacement = Span(), std::string insertion = "", std::string sort_key = "")
        : label(std::move(label)), kind(kind), detail(std::move(detail)),
          replacement(replacement), insertion(insertion.empty() ? this->label : std::move(insertion)),
          sort_key(sort_key.empty() ? this->label : std::move(sort_key)) {}
};

export class Hover {
  public:
    std::string contents;
    Span span;

    Hover(std::string contents, Span span) : contents(std::move(contents)), span(span) {}
};

export class SemanticKind {
  public:
    enum class Type : std::uint8_t {
        Type,
        Enum,
        Interface,
        Struct,
        TypeParameter,
        Parameter,
        Variable,
        Property,
        EnumMember,
        Function,
        Method,
        Keyword,
        Modifier,
        String,
        Number,
        Operator,
    };
};

export class SemanticModifier {
  public:
    static constexpr std::uint32_t None = 0;
    static constexpr std::uint32_t Declaration = 1U << 0U;
    static constexpr std::uint32_t Definition = 1U << 1U;
    static constexpr std::uint32_t Readonly = 1U << 2U;
    static constexpr std::uint32_t Static = 1U << 3U;
};

export class SemanticToken {
  public:
    Span span;
    SemanticKind::Type kind;
    std::uint32_t modifiers;

    SemanticToken(Span span, SemanticKind::Type kind,
                  std::uint32_t modifiers = SemanticModifier::None)
        : span(span), kind(kind), modifiers(modifiers) {}
};

export class DocumentHighlight {
  public:
    class Kind {
      public:
        enum class Type : std::uint8_t { Text, Read, Write };
    };

    Span span;
    Kind::Type kind;

    DocumentHighlight(Span span, Kind::Type kind = Kind::Type::Read) : span(span), kind(kind) {}
};

export class DocumentSymbol {
  public:
    std::string name;
    CompletionKind::Type kind;
    AnalysisLocation location;
    Span selection;
    std::vector<DocumentSymbol> children;

    DocumentSymbol(std::string name, CompletionKind::Type kind, AnalysisLocation location,
                   Span selection, std::vector<DocumentSymbol> children = {})
        : name(std::move(name)), kind(kind), location(std::move(location)), selection(selection),
          children(std::move(children)) {}
};

export class SignatureParameter {
  public:
    std::string label;

    explicit SignatureParameter(std::string label) : label(std::move(label)) {}
};

export class Signature {
  public:
    std::string label;
    std::vector<SignatureParameter> parameters;

    Signature(std::string label, std::vector<SignatureParameter> parameters)
        : label(std::move(label)), parameters(std::move(parameters)) {}
};

export class SignatureHelp {
  public:
    std::vector<Signature> signatures;
    std::size_t active_signature;
    std::size_t active_parameter;

    SignatureHelp(std::vector<Signature> signatures, std::size_t active_signature,
                  std::size_t active_parameter)
        : signatures(std::move(signatures)), active_signature(active_signature),
          active_parameter(active_parameter) {}
};
