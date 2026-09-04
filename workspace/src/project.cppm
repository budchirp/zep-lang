module;

#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

export module zep.workspace.project;

import zep.common.diagnostic.collection;
import zep.workspace.manifest;
import zep.workspace.module_path;
import zep.workspace.package;
import zep.workspace.package.graph;
import zep.workspace.toolchain;

export class Project {
  public:
    PackageGraph packages;
    Package* root_package;
    std::filesystem::path root_directory;

    Project(PackageGraph packages, Package* root_package, std::filesystem::path root_directory)
        : packages(std::move(packages)), root_package(root_package),
          root_directory(std::move(root_directory)) {}

    static std::optional<std::filesystem::path> find_root(const std::filesystem::path& path) {
        ManifestReader reader;
        auto manifest = reader.find(path);
        if (!manifest.has_value()) {
            return std::nullopt;
        }

        return manifest->parent_path();
    }

    static std::optional<ModulePath> module_path(const std::filesystem::path& project_root,
                                                 const std::filesystem::path& source_path) {
        auto source_directory = project_root / "src";
        std::error_code error_code;
        auto canonical_source_directory =
            std::filesystem::weakly_canonical(source_directory, error_code);
        auto canonical_source = std::filesystem::weakly_canonical(source_path, error_code);

        auto relative = canonical_source.lexically_relative(canonical_source_directory);
        if (relative.empty() || relative.string().starts_with("..")) {
            return std::nullopt;
        }

        std::vector<std::string> segments;
        for (const auto& part : relative) {
            segments.push_back(part.string());
        }

        if (segments.empty()) {
            return std::nullopt;
        }

        auto last = segments.back();
        if (last.ends_with(".zep")) {
            segments.back() = last.substr(0, last.length() - 4);
        }

        return ModulePath(std::move(segments));
    }

    static std::optional<Project> open(const std::filesystem::path& path, Diagnostics& diagnostics,
                                       const Toolchain& toolchain = Toolchain::discover()) {
        auto root = find_root(path);
        if (!root.has_value()) {
            return std::nullopt;
        }

        ManifestReader reader;
        auto manifest = reader.read(*root / "zep.json");
        if (!manifest.has_value()) {
            return std::nullopt;
        }

        PackageResolver resolver(toolchain);
        auto resolved_packages = resolver.resolve(*root, diagnostics);
        if (!resolved_packages.has_value()) {
            return std::nullopt;
        }

        auto* package = resolved_packages->find(manifest->name);
        if (package == nullptr) {
            return std::nullopt;
        }

        return Project(std::move(*resolved_packages), package, std::move(*root));
    }
};
