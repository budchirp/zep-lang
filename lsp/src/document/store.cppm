module;

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

export module zep.lsp.document.store;

import zep.lsp.document;

export class DocumentStore {
  private:
    std::unordered_map<std::string, Document> documents;

  public:
    DocumentStore() = default;

    void open(std::string uri, std::int32_t version, std::string text) {
        documents.insert_or_assign(uri, Document(uri, version, std::move(text)));
    }

    void update(std::string uri, std::int32_t version, std::string text) {
        documents.insert_or_assign(uri, Document(uri, version, std::move(text)));
    }

    void close(const std::string& uri) { documents.erase(uri); }

    const Document* find(const std::string& uri) const {
        auto iterator = documents.find(uri);
        if (iterator == documents.end()) {
            return nullptr;
        }

        return &iterator->second;
    }

    std::size_t size() const { return documents.size(); }
};
