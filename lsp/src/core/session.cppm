module;

#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module zep.lsp.session;

import zep.lsp.analysis.analyzer;
import zep.lsp.analysis.snapshot;
import zep.lsp.analysis.types;
import zep.common.source.position;
import zep.lsp.document;
import zep.lsp.transport;

export class Session {
  private:
    std::unordered_map<std::string, Document> documents;
    std::unordered_map<std::string, std::unique_ptr<Analysis>> analyses;
    Analyzer analyzer;
    Transport& transport;
    std::filesystem::path workspace_root;

    void invalidate() { analyses.clear(); }

  public:
    enum class State { Created, Initialized, Shutdown, Exited };

    State state = State::Created;
    int exit_code = 0;

    Session(Transport& transport, std::filesystem::path standard_library)
        : analyzer(std::move(standard_library)), transport(transport) {}

    void initialize(std::optional<std::string> root_uri) {
        if (root_uri.has_value() && !root_uri->empty()) {
            workspace_root = document_path(*root_uri);
        }
        state = State::Initialized;
    }

    void open(std::string uri, std::int64_t version, std::string content) {
        Document document(std::move(uri), version, std::move(content));
        analyzer.overlay(document.path, document.text);
        if (workspace_root.empty()) {
            workspace_root = document.path.parent_path();
        }
        documents.insert_or_assign(document.uri, std::move(document));
        invalidate();
    }

    bool change(const std::string& uri, std::int64_t version, const nlohmann::json& changes) {
        auto iterator = documents.find(uri);
        if (iterator == documents.end()) {
            throw std::invalid_argument("document is not open");
        }
        if (!iterator->second.change(version, changes)) {
            return false;
        }
        analyzer.overlay(iterator->second.path, iterator->second.text);
        invalidate();
        return true;
    }

    void close(const std::string& uri) {
        auto iterator = documents.find(uri);
        if (iterator == documents.end()) {
            return;
        }
        analyzer.remove_overlay(iterator->second.path);
        documents.erase(iterator);
        invalidate();
    }

    Document* document(const std::string& uri) {
        auto iterator = documents.find(uri);
        return iterator != documents.end() ? &iterator->second : nullptr;
    }

    const Document* document(const std::string& uri) const {
        auto iterator = documents.find(uri);
        return iterator != documents.end() ? &iterator->second : nullptr;
    }

    std::vector<const Document*> open_documents() const {
        std::vector<const Document*> result;
        result.reserve(documents.size());
        for (const auto& [_, document] : documents) {
            result.push_back(&document);
        }
        return result;
    }

    Analysis& analysis(const std::string& uri) {
        auto* opened = document(uri);
        if (opened == nullptr) {
            throw std::invalid_argument("document is not open");
        }
        if (auto iterator = analyses.find(uri); iterator != analyses.end()) {
            return *iterator->second;
        }
        auto result = std::make_unique<Analysis>(analyzer.analyze(opened->path));
        auto* value = result.get();
        analyses.emplace(uri, std::move(result));
        return *value;
    }

    std::vector<Completion> complete(const std::string& uri, Position position) {
        auto* opened = document(uri);
        return opened != nullptr ? analyzer.complete(opened->path, position)
                                 : std::vector<Completion>();
    }

    std::vector<DocumentSymbol> workspace_symbols(const std::string& query) {
        if (workspace_root.empty()) {
            return {};
        }
        return analyzer.workspace_symbols(workspace_root, query);
    }

    std::string content(const std::filesystem::path& path) const {
        auto normalized_path = std::filesystem::absolute(path).lexically_normal();
        for (const auto& [_, document] : documents) {
            if (std::filesystem::absolute(document.path).lexically_normal() == normalized_path) {
                return document.text;
            }

            std::error_code error_code;
            if (std::filesystem::equivalent(document.path, path, error_code) && !error_code) {
                return document.text;
            }
        }

        std::ifstream input(path);
        std::ostringstream output;
        output << input.rdbuf();
        return output.str();
    }

    void write(const nlohmann::json& message) { transport.write(message); }
};
