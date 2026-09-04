module;

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

export module zep.compiler.resolver;

import zep.workspace.module_path;
import zep.workspace.package;
import zep.workspace.package.graph;

export class ResolvedModule {
  public:
    Package* owner;
    ModulePath path;
    std::filesystem::path source_path;

    ResolvedModule(Package* owner, ModulePath path, std::filesystem::path source_path)
        : owner(owner), path(std::move(path)), source_path(std::move(source_path)) {}
};

export class ModuleResolver {
  private:
    static std::optional<std::filesystem::path>
    find_source(const std::filesystem::path& root, const ModulePath& path, bool root_module) {
        auto base = root / "src";
        if (!root_module) {
            base /= path.path();
        }

        auto file = base;
        file += ".zep";
        if (std::filesystem::is_regular_file(file)) {
            return file;
        }

        auto index = base / "index.zep";
        if (std::filesystem::is_regular_file(index)) {
            return index;
        }

        return std::nullopt;
    }

    static std::optional<std::filesystem::path> find_dependency_source(const Package& package,
                                                                       const ModulePath& path) {
        if (path.segments.empty()) {
            return std::nullopt;
        }

        if (path.segments.size() == 1) {
            auto root = package.root / "src" / "lib.zep";
            if (std::filesystem::is_regular_file(root)) {
                return root;
            }
        }

        std::vector<std::string> segments;
        segments.reserve(path.segments.size() - 1);
        for (std::size_t index = 1; index < path.segments.size(); ++index) {
            segments.push_back(path.segments[index]);
        }

        if (segments.empty()) {
            return std::nullopt;
        }

        return find_source(package.root, ModulePath(std::move(segments)), false);
    }

  public:
    std::optional<ResolvedModule> resolve(Package& importing, ModulePath path) const {
        auto local_path = path;
        if (local_path.segments.size() > 1 &&
            local_path.segments.front() == importing.manifest.name) {
            local_path.segments.erase(local_path.segments.begin());
        }
        if (auto local = find_source(importing.root, local_path, false); local.has_value()) {
            return ResolvedModule(&importing, local_path, std::move(*local));
        }

        if (path.segments.empty()) {
            return std::nullopt;
        }

        for (auto* dependency : importing.dependencies) {
            if (dependency->manifest.name != path.segments.front()) {
                continue;
            }

            if (auto source = find_dependency_source(*dependency, path); source.has_value()) {
                std::vector<std::string> module_segments;
                if (path.segments.size() == 1) {
                    module_segments.push_back("lib");
                } else {
                    module_segments.reserve(path.segments.size() - 1);
                    for (std::size_t index = 1; index < path.segments.size(); ++index) {
                        module_segments.push_back(path.segments[index]);
                    }
                }
                return ResolvedModule(dependency, ModulePath(std::move(module_segments)),
                                      std::move(*source));
            }
        }

        return std::nullopt;
    }
};
