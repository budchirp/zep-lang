module;

#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

export module zep.workspace.module_path;

export class ModulePath {
  public:
    std::vector<std::string> segments;

    explicit ModulePath(std::vector<std::string> segments) : segments(std::move(segments)) {
        validate();
    }

    static ModulePath from_string(const std::string& value) {
        std::vector<std::string> values;
        std::size_t begin = 0;
        while (begin <= value.size()) {
            auto end = value.find('.', begin);
            auto segment = value.substr(begin, end == std::string::npos ? end : end - begin);
            if (segment.empty()) {
                throw std::invalid_argument("module path contains an empty segment");
            }
            values.push_back(std::move(segment));
            if (end == std::string::npos) {
                break;
            }
            begin = end + 1;
        }
        return ModulePath(std::move(values));
    }

    static bool valid_segment(const std::string& segment) {
        if (segment.empty() || ((segment.front() >= '0') && (segment.front() <= '9'))) {
            return false;
        }
        for (const auto character : segment) {
            if (!((character >= 'a' && character <= 'z') ||
                  (character >= 'A' && character <= 'Z') ||
                  (character >= '0' && character <= '9') || character == '_')) {
                return false;
            }
        }
        return true;
    }

    void validate() const {
        if (segments.empty()) {
            throw std::invalid_argument("module path cannot be empty");
        }
        for (const auto& segment : segments) {
            if (!valid_segment(segment)) {
                throw std::invalid_argument("invalid module path segment: " + segment);
            }
        }
    }

    std::string string() const {
        std::string result;

        for (const auto& segment : segments) {
            if (!result.empty()) {
                result.push_back('.');
            }

            result += segment;
        }

        return result;
    }

    std::filesystem::path path() const {
        std::filesystem::path result;
        for (const auto& segment : segments) {
            result /= segment;
        }
        return result;
    }

    std::filesystem::path source_path(const std::filesystem::path& root) const {
        return root / path().replace_extension(".zep");
    }

    bool operator==(const ModulePath& other) const = default;
};
