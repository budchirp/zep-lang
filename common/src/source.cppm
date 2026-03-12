module;

#include <string>
#include <string_view>

export module zep.common.source;

export class Source {
  public:
    std::string name;
    std::string_view content;

    Source(std::string name, std::string_view content) : name(std::move(name)), content(content) {}
};
