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
    find_source(const std::filesystem::path& source_directory, const ModulePath& path) {
        auto base = source_directory / path.path();

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

  public:
    std::optional<ResolvedModule> resolve(Package& importing, ModulePath path) const {
        auto local_path = path;
        if (local_path.segments.size() > 1 &&
            local_path.segments.front() == importing.manifest.name) {
            local_path.segments.erase(local_path.segments.begin());
        }
        if (auto local = find_source(importing.source_directory, local_path);
            local.has_value()) {
            return ResolvedModule(&importing, local_path, std::move(*local));
        }

        if (path.segments.empty()) {
            return std::nullopt;
        }

        for (auto* dependency : importing.dependencies) {
            if (dependency->manifest.name != path.segments.front()) {
                continue;
            }

            if (path.segments.size() == 1) {
                auto source = dependency->source_directory / "lib.zep";
                if (std::filesystem::is_regular_file(source)) {
                    return ResolvedModule(dependency, ModulePath::from_string("lib"),
                                          std::move(source));
                }
                continue;
            }

            std::vector<std::string> segments;
            segments.reserve(path.segments.size() - 1);
            for (auto index = std::size_t{1}; index < path.segments.size(); ++index) {
                segments.push_back(path.segments[index]);
            }

            ModulePath module_path(std::move(segments));
            if (auto source = find_source(dependency->source_directory, module_path);
                source.has_value()) {
                return ResolvedModule(dependency, std::move(module_path), std::move(*source));
            }
        }

        return std::nullopt;
    }
};
