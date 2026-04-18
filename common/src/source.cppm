module;

#include <string>

export module zep.common.source;

export class Source {
  public:
    std::string name;
    std::string content;

    Source(std::string name, std::string content)
        : name(std::move(name)), content(std::move(content)) {}
};
