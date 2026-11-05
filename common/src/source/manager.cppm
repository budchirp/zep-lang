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
import zep.common.paths;

export class SourceManager {
  private:
    std::vector<std::unique_ptr<Source>> sources;

    Source* find(const std::string& name) const {
        for (const auto& source : sources) {
            if (source->name == name) {
                return source.get();
            }
        }

        return nullptr;
    }

  public:
    Source& add(std::string name, std::string content) {
        auto source = std::make_unique<Source>(std::move(name), std::move(content));
        auto* ptr = source.get();
        sources.push_back(std::move(source));
        return *ptr;
    }

    Source& add_override(const std::filesystem::path& path, std::string content) {
        auto name = Paths::canonical(path).string();

        if (auto* source = find(name); source != nullptr) {
            source->content = std::move(content);
            return *source;
        }

        return add(std::move(name), std::move(content));
    }

    std::optional<Source*> load(const std::filesystem::path& path) {
        auto name = Paths::canonical(path).string();

        if (auto* source = find(name); source != nullptr) {
            return source;
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            return std::nullopt;
        }

        std::ostringstream buffer;
        buffer << file.rdbuf();

        return &add(std::move(name), buffer.str());
    }
};
