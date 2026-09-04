module;

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

export module zep.common.source.manager;

import zep.common.source;

export class SourceManager {
  private:
    std::vector<std::unique_ptr<Source>> sources;

  public:
    Source& add(std::string name, std::string content) {
        auto source = std::make_unique<Source>(std::move(name), std::move(content));
        auto* ptr = source.get();
        sources.push_back(std::move(source));
        return *ptr;
    }

    Source& add_override(const std::filesystem::path& path, std::string content) {
        auto canonical_path = std::filesystem::weakly_canonical(path);
        auto canonical_string = canonical_path.string();
        for (const auto& existing : sources) {
            if (existing->name == canonical_string) {
                existing->content = std::move(content);
                return *existing;
            }
        }
        return add(std::move(canonical_string), std::move(content));
    }

    std::optional<Source*> load(const std::filesystem::path& path) {
        auto canonical_path = std::filesystem::weakly_canonical(path);
        auto canonical_string = canonical_path.string();
        for (const auto& existing : sources) {
            if (existing->name == canonical_string) {
                return existing.get();
            }
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            return std::nullopt;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();

        return &add(std::move(canonical_string), buffer.str());
    }
};
