module;

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

export module zep.lsp.analysis;

import zep.common.diagnostic.collection;
import zep.common.diagnostic.diagnostic;
import zep.compiler.analysis;
import zep.frontend.node;
import zep.frontend.node.program;
import zep.frontend.sema.kind;
import zep.frontend.sema.resolver.builtin;
import zep.frontend.sema.scope;
import zep.lsp.analysis.completer;
import zep.lsp.analysis.finder;
import zep.lsp.analysis.tokens;
import zep.lsp.protocol.types;

export class AnalysisService {
  private:
    CompilerAnalysis analysis;

    static std::filesystem::path uri_to_path(const std::string& uri) {
        if (uri.starts_with("file://")) {
            return std::filesystem::path(uri.substr(7));
        }

        return std::filesystem::path(uri);
    }

    static std::optional<lsp::Hover> format_hover_for_node(Node* node, const Scope* scope) {
        if (node == nullptr) {
            return std::nullopt;
        }

        auto range = lsp::PositionConverter::from_compiler_span(node->span);

        if (auto* id = node->as<IdentifierExpression>(); id != nullptr) {
            if (id->var_symbol != nullptr && id->var_symbol->type != nullptr) {
                return lsp::Hover("```zep\nvar " + id->name + ": " +
                                      id->var_symbol->type->to_string() + "\n```",
                                  range);
            }

            if (id->function_symbol != nullptr && id->function_symbol->function_type != nullptr) {
                return lsp::Hover("```zep\nfn " + id->name + ": " +
                                      id->function_symbol->function_type->to_string() + "\n```",
                                  range);
            }

            if (id->type != nullptr) {
                return lsp::Hover("```zep\n" + id->name + ": " + id->type->to_string() + "\n```",
                                  range);
            }

            if (scope != nullptr) {
                if (const auto* var = scope->lookup_var(id->name);
                    var != nullptr && var->type != nullptr) {
                    return lsp::Hover(
                        "```zep\nvar " + id->name + ": " + var->type->to_string() + "\n```", range);
                }

                if (const auto* fn = scope->lookup_function(id->name);
                    fn != nullptr && fn->function_type != nullptr) {
                    return lsp::Hover("```zep\nfn " + id->name + ": " +
                                          fn->function_type->to_string() + "\n```",
                                      range);
                }

                if (const auto* ty = scope->lookup_type(id->name);
                    ty != nullptr && ty->type != nullptr) {
                    return lsp::Hover(
                        "```zep\ntype " + id->name + ": " + ty->type->to_string() + "\n```", range);
                }
            }

            return lsp::Hover("```zep\n" + id->name + "\n```", range);
        }

        if (auto* var = node->as<VarDeclaration>(); var != nullptr) {
            auto type_text = var->type != nullptr ? var->type->to_string() : "unknown";
            return lsp::Hover("```zep\nvar " + var->name + ": " + type_text + "\n```", range);
        }

        if (auto* function = node->as<FunctionDeclaration>(); function != nullptr) {
            auto type_text = function->function_symbol != nullptr &&
                                     function->function_symbol->function_type != nullptr
                                 ? function->function_symbol->function_type->to_string()
                                 : "fn " + function->prototype->name;
            return lsp::Hover(
                "```zep\nfn " + function->prototype->name + ": " + type_text + "\n```", range);
        }

        if (auto* parameter = node->as<Parameter>(); parameter != nullptr) {
            auto type_text = (parameter->type != nullptr && parameter->type->type != nullptr)
                                 ? parameter->type->type->to_string()
                                 : "";
            return lsp::Hover("```zep\n(parameter) " + parameter->name +
                                  (type_text.empty() ? "" : ": " + type_text) + "\n```",
                              range);
        }

        if (auto* field = node->as<Field>(); field != nullptr) {
            auto type_text = (field->type != nullptr && field->type->type != nullptr)
                                 ? field->type->type->to_string()
                                 : "";
            return lsp::Hover("```zep\n(field) " + field->name +
                                  (type_text.empty() ? "" : ": " + type_text) + "\n```",
                              range);
        }

        if (auto* prototype = node->as<FunctionPrototype>(); prototype != nullptr) {
            auto type_text =
                prototype->return_type != nullptr && prototype->return_type->type != nullptr
                    ? prototype->return_type->type->to_string()
                    : "void";
            return lsp::Hover("```zep\nfn " + prototype->name + ": " + type_text + "\n```", range);
        }

        if (auto* structure = node->as<StructDeclaration>(); structure != nullptr) {
            return lsp::Hover("```zep\nstruct " + structure->name + "\n```", range);
        }

        if (auto* enumeration = node->as<EnumDeclaration>(); enumeration != nullptr) {
            return lsp::Hover("```zep\nenum " + enumeration->name + "\n```", range);
        }

        if (auto* expression = dynamic_cast<Expression*>(node);
            expression != nullptr && expression->type != nullptr) {
            return lsp::Hover("```zep\n" + expression->type->to_string() + "\n```", range);
        }

        return std::nullopt;
    }

  public:
    AnalysisService() = default;

    void update_document(const std::string& uri, const std::string& content) {
        analysis.set_source_override(uri_to_path(uri), content);
    }

    void close_document(const std::string& uri) {
        analysis.remove_source_override(uri_to_path(uri));
    }

    std::vector<lsp::Diagnostic> analyze(const std::string& uri, const std::string& content) {
        auto path = uri_to_path(uri);
        auto result = analysis.analyze(path, content);

        std::vector<lsp::Diagnostic> diagnostics;
        diagnostics.reserve(result.diagnostics().size());

        for (const auto& entry : result.diagnostics()) {
            auto range = lsp::PositionConverter::from_compiler_span(entry.location.span);
            auto severity = entry.severity == DiagnosticSeverity::Type::Error
                                ? lsp::DiagnosticSeverity::Type::Error
                                : lsp::DiagnosticSeverity::Type::Warning;
            diagnostics.emplace_back(range, severity, entry.message, "zep", entry.code);
        }

        return diagnostics;
    }

    std::optional<lsp::Hover> hover(const std::string& uri, const std::string& content,
                                    lsp::Position position) {
        auto path = uri_to_path(uri);
        auto result = analysis.analyze(path, content);
        if (!result.is_valid()) {
            return std::nullopt;
        }

        auto compiler_position = lsp::PositionConverter::to_compiler(position);
        NodeFinder finder(compiler_position);
        for (auto* statement : result.program()->statements) {
            finder.visit_child(statement);
        }

        if (finder.innermost == nullptr) {
            return std::nullopt;
        }

        return format_hover_for_node(finder.innermost, result.scope());
    }

    std::vector<lsp::CompletionItem> complete(const std::string& uri, const std::string& content,
                                              lsp::Position position) {
        auto path = uri_to_path(uri);

        std::size_t offset = 0;
        std::size_t cur_line = 0;
        std::size_t cur_col = 0;
        for (std::size_t i = 0; i < content.size(); ++i) {
            if (cur_line == position.line && cur_col == position.character) {
                offset = i;
                break;
            }
            if (content[i] == '\n') {
                ++cur_line;
                cur_col = 0;
            } else {
                ++cur_col;
            }
        }
        if (cur_line == position.line && cur_col == position.character) {
            offset = content.size();
        }

        std::string analysis_content = content;
        std::size_t check_pos = offset;
        while (check_pos > 0 && (std::isalnum(static_cast<unsigned char>(content[check_pos - 1])) != 0 ||
                                 content[check_pos - 1] == '_')) {
            --check_pos;
        }

        if (check_pos > 0 && content[check_pos - 1] == '.') {
            if (check_pos == offset) {
                analysis_content.insert(offset, "__zep_member_dummy;");
            }
        }

        auto result = analysis.analyze(path, analysis_content);
        return Completer::complete(content, position, result.program(), result.scope());
    }

    lsp::SemanticTokens semantic_tokens(const std::string& uri, const std::string& content) {
        auto path = uri_to_path(uri);
        auto result = analysis.analyze(path, content);
        return SemanticTokenExtractor::extract(content, result.program());
    }
};
