module;

#include <cstdint>
#include <string>
#include <utility>

export module zep.lsp.document;

export class Document {
  private:
  public:
    std::string uri;
    std::int32_t version;
    std::string content;

    Document(std::string uri, std::int32_t version, std::string content)
        : uri(std::move(uri)), version(version), content(std::move(content)) {}
};
