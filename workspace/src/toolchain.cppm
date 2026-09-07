module;

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

export module zep.workspace.toolchain;

export class Toolchain {
  private:
    static void append_unique(std::vector<std::filesystem::path>& paths,
                              std::filesystem::path path) {
        if (path.empty()) {
            return;
        }
        for (const auto& existing : paths) {
            if (existing == path) {
                return;
            }
        }
        paths.push_back(std::move(path));
    }

    static std::vector<std::filesystem::path> discover_package_roots() {
        std::vector<std::filesystem::path> paths;
        paths.reserve(3);
        if (const auto* value = std::getenv("ZEP_LIBS_DIR"); value != nullptr) {
            append_unique(paths, value);
        }
        if (const auto* value = std::getenv("HOME"); value != nullptr) {
            append_unique(paths, std::filesystem::path(value) / ".local/share/zep/libs");
        }
        try {
            auto executable = std::filesystem::canonical("/proc/self/exe");
            append_unique(paths, executable.parent_path().parent_path() / "share/zep/libs");
        } catch (const std::exception&) {}
        return paths;
    }

    static std::filesystem::path find_tool(const std::string& name) {
        if (const auto* value = std::getenv("PATH"); value != nullptr) {
            std::string path_value(value);
            std::size_t begin = 0;
            while (begin <= path_value.size()) {
                auto end = path_value.find(':', begin);
                auto directory =
                    path_value.substr(begin, end == std::string::npos ? end : end - begin);
                auto candidate = std::filesystem::path(directory) / name;
                if (std::filesystem::is_regular_file(candidate)) {
                    return candidate;
                }
                if (end == std::string::npos) {
                    break;
                }
                begin = end + 1;
            }
        }
        return name;
    }

  public:
    std::filesystem::path compiler;
    std::filesystem::path standard_library;
    std::vector<std::filesystem::path> package_roots;

    Toolchain(std::filesystem::path compiler, std::filesystem::path standard_library,
              std::vector<std::filesystem::path> package_roots)
        : compiler(std::move(compiler)), standard_library(std::move(standard_library)),
          package_roots(std::move(package_roots)) {}

    static Toolchain discover() {
        auto package_roots = discover_package_roots();
        auto standard_library = std::filesystem::path();
        for (const auto& root : package_roots) {
            auto candidate = root / "std";
            if (std::filesystem::is_regular_file(candidate / "zep.json")) {
                standard_library = candidate;
                break;
            }
            if (std::filesystem::is_directory(candidate)) {
                std::error_code error_code;
                for (const auto& entry :
                     std::filesystem::directory_iterator(candidate, error_code)) {
                    if (entry.is_directory() &&
                        std::filesystem::is_regular_file(entry.path() / "zep.json")) {
                        standard_library = entry.path();
                        break;
                    }
                }
                if (!standard_library.empty()) {
                    break;
                }
            }
        }
        return Toolchain{find_tool("clang"), std::move(standard_library), std::move(package_roots)};
    }
};
