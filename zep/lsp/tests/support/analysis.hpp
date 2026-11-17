#pragma once

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <utility>
#include <vector>

import zep.common.diagnostic.diagnostic;
import zep.common.source.position;
import zep.lsp.analysis.analyzer;
import zep.lsp.analysis.snapshot;
import zep.lsp.analysis.types;
import zep.test.support;
import zep.common.diagnostic.collection;

class AnalysisHarness {
  private:
    Analyzer analyzer;

    static std::filesystem::path path(const std::string& uri) {
        if (uri.starts_with("file://")) {
            return std::filesystem::path(uri.substr(7));
        }

        return std::filesystem::path(uri);
    }

  public:
    AnalysisHarness() = default;

    explicit AnalysisHarness(std::filesystem::path standard_library)
        : analyzer(std::move(standard_library)) {}

    std::vector<Diagnostic> analyze(const std::string& uri, const std::string& content) {
        return analyzer.analyze(path(uri), content).diagnostics();
    }

    std::optional<Hover> hover(const std::string& uri, const std::string& content,
                               Position position) {
        return analyzer.analyze(path(uri), content).hover(
            Position(position.line + 1, position.column + 1));
    }

    std::vector<Completion> complete(const std::string& uri, const std::string& content,
                                     Position position) {
        analyzer.overlay(path(uri), content);

        return analyzer.complete(path(uri), Position(position.line + 1, position.column + 1));
    }

    std::vector<Completion> complete_cursor(std::string content,
                                           const std::string& filename = "completion_context.zep") {
        const auto cursor = content.find('|');

        if (cursor == std::string::npos) {
            ADD_FAILURE() << "completion fixture has no cursor";

            return {};
        }

        Position position;

        for (auto index = std::size_t{0}; index < cursor; ++index) {
            if (content[index] == '\n') {
                ++position.line;
                position.column = 0;
            } else {
                ++position.column;
            }
        }

        content.erase(cursor, 1);

        return complete("file:///" + filename, content, position);
    }

    std::vector<SemanticToken> semantic_tokens(const std::string& uri, const std::string& content) {
        return analyzer.analyze(path(uri), content).tokens();
    }
};

inline const Completion* find_completion(const std::vector<Completion>& items, const std::string& name) {
    const auto found = std::ranges::find(items, name, &Completion::label);

    return found != items.end() ? &*found : nullptr;
}
