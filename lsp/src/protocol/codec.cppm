module;

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

export module zep.lsp.protocol;

import zep.common.diagnostic.diagnostic;
import zep.common.source.position;
import zep.common.source.span;
import zep.lsp.analysis.types;
import zep.lsp.document;
import zep.lsp.session;

export class ProtocolCodec {
  private:
    Session& session;

    std::uint32_t completion_kind(CompletionKind::Type kind) const {
        switch (kind) {
        case CompletionKind::Type::Method: return 2;
        case CompletionKind::Type::Function: return 3;
        case CompletionKind::Type::Field: return 5;
        case CompletionKind::Type::Variable: return 6;
        case CompletionKind::Type::Interface: return 8;
        case CompletionKind::Type::Module: return 9;
        case CompletionKind::Type::Enum: return 13;
        case CompletionKind::Type::Keyword: return 14;
        case CompletionKind::Type::EnumMember: return 20;
        case CompletionKind::Type::Constant: return 21;
        case CompletionKind::Type::Struct: return 22;
        case CompletionKind::Type::TypeParameter: return 25;
        }

        return 1;
    }

    std::uint32_t semantic_kind(SemanticKind::Type kind) const {
        switch (kind) {
        case SemanticKind::Type::Type: return 0;
        case SemanticKind::Type::Enum: return 2;
        case SemanticKind::Type::Interface: return 3;
        case SemanticKind::Type::Struct: return 4;
        case SemanticKind::Type::TypeParameter: return 5;
        case SemanticKind::Type::Parameter: return 6;
        case SemanticKind::Type::Variable: return 7;
        case SemanticKind::Type::Property: return 8;
        case SemanticKind::Type::EnumMember: return 9;
        case SemanticKind::Type::Function: return 10;
        case SemanticKind::Type::Method: return 11;
        case SemanticKind::Type::Keyword: return 12;
        case SemanticKind::Type::Modifier: return 13;
        case SemanticKind::Type::String: return 15;
        case SemanticKind::Type::Number: return 16;
        case SemanticKind::Type::Operator: return 17;
        }

        return 0;
    }

    nlohmann::json symbol(const Document& document, const DocumentSymbol& value) const {
        nlohmann::json result = {
            {"name", value.name},
            {"kind", completion_kind(value.kind)},
            {"range", range(document, value.location.span)},
            {"selectionRange", range(document, value.selection)},
        };

        if (!value.children.empty()) {
            result["children"] = nlohmann::json::array();
            for (const auto& child : value.children) {
                result["children"].push_back(symbol(document, child));
            }
        }

        return result;
    }

  public:
    explicit ProtocolCodec(Session& session) : session(session) {}

    Position position(const Document& document, const nlohmann::json& value) const {
        return document.position(value);
    }

    nlohmann::json range(const Document& document, Span span) const {
        auto convert = [&document](Position position) {
            return nlohmann::json{
                {"line", position.line > 0 ? position.line - 1 : 0},
                {"character", position.line > 0 ? document.utf16_character(position) : 0},
            };
        };

        return {{"start", convert(span.start)}, {"end", convert(span.end)}};
    }

    nlohmann::json location(const AnalysisLocation& value) const {
        Document document(document_uri(value.path), 0, session.content(value.path));
        return {{"uri", document.uri}, {"range", range(document, value.span)}};
    }

    nlohmann::json diagnostics(const Document& document,
                               const std::vector<Diagnostic>& values) const {
        nlohmann::json entries = nlohmann::json::array();
        for (const auto& diagnostic : values) {
            nlohmann::json entry = {
                {"range", range(document, diagnostic.location.span)},
                {"severity", diagnostic.severity == DiagnosticSeverity::Type::Error ? 1 : 2},
                {"message", diagnostic.message},
                {"source", "zep"},
            };

            if (!diagnostic.code.empty()) {
                entry["code"] = diagnostic.code;
            }

            entries.push_back(std::move(entry));
        }

        return entries;
    }

    nlohmann::json completions(const Document& document,
                               const std::vector<Completion>& values) const {
        nlohmann::json items = nlohmann::json::array();
        for (const auto& completion : values) {
            nlohmann::json item = {
                {"label", completion.label},
                {"kind", completion_kind(completion.kind)},
                {"sortText", completion.sort_key},
            };

            if (completion.replacement.start.line > 0) {
                item["textEdit"] = {
                    {"range", range(document, completion.replacement)},
                    {"newText", completion.insertion},
                };
            }

            if (!completion.detail.empty()) {
                item["detail"] = completion.detail;
            }

            items.push_back(std::move(item));
        }

        return {{"isIncomplete", false}, {"items", std::move(items)}};
    }

    nlohmann::json tokens(const Document& document,
                          const std::vector<SemanticToken>& values) const {
        std::vector<std::uint32_t> data;
        data.reserve(values.size() * 5);

        std::uint32_t previous_line = 0;
        std::uint32_t previous_character = 0;
        for (const auto& token : values) {
            auto line = static_cast<std::uint32_t>(token.span.start.line - 1);
            auto character = static_cast<std::uint32_t>(
                document.utf16_character(token.span.start));
            auto end_character = static_cast<std::uint32_t>(
                document.utf16_character(token.span.end));
            auto length = end_character >= character ? end_character - character + 1 : 0;
            auto line_delta = line - previous_line;
            auto character_delta = line_delta == 0 ? character - previous_character : character;

            data.push_back(line_delta);
            data.push_back(character_delta);
            data.push_back(length);
            data.push_back(semantic_kind(token.kind));
            data.push_back(token.modifiers);

            previous_line = line;
            previous_character = character;
        }

        return {{"data", std::move(data)}};
    }

    nlohmann::json highlights(const Document& document,
                              const std::vector<DocumentHighlight>& values) const {
        nlohmann::json result = nlohmann::json::array();
        for (const auto& highlight : values) {
            auto kind = highlight.kind == DocumentHighlight::Kind::Type::Write
                            ? 3
                            : highlight.kind == DocumentHighlight::Kind::Type::Read ? 2 : 1;
            result.push_back({{"range", range(document, highlight.span)}, {"kind", kind}});
        }

        return result;
    }

    nlohmann::json document_symbols(const Document& document,
                                    const std::vector<DocumentSymbol>& values) const {
        nlohmann::json result = nlohmann::json::array();
        for (const auto& value : values) {
            result.push_back(symbol(document, value));
        }

        return result;
    }

    nlohmann::json workspace_symbols(const std::vector<DocumentSymbol>& values) const {
        nlohmann::json result = nlohmann::json::array();
        for (const auto& value : values) {
            result.push_back({
                {"name", value.name},
                {"kind", completion_kind(value.kind)},
                {"location", location(value.location)},
            });
        }

        return result;
    }

    nlohmann::json signature(const SignatureHelp& help) const {
        nlohmann::json signatures = nlohmann::json::array();
        for (const auto& value : help.signatures) {
            nlohmann::json parameters = nlohmann::json::array();
            for (const auto& parameter : value.parameters) {
                parameters.push_back({{"label", parameter.label}});
            }

            signatures.push_back({
                {"label", value.label},
                {"parameters", std::move(parameters)},
            });
        }

        return {
            {"signatures", std::move(signatures)},
            {"activeSignature", help.active_signature},
            {"activeParameter", help.active_parameter},
        };
    }
};
