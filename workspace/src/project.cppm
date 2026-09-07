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

    std::optional<ModulePath> module(const std::filesystem::path& source_path) const {
        std::error_code error_code;
        auto canonical_source_directory =
            std::filesystem::weakly_canonical(root_package->source_directory, error_code);
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
        ManifestReader reader;
        auto manifest_path = reader.find(path);
        if (!manifest_path.has_value()) {
            return std::nullopt;
        }

        auto root = manifest_path->parent_path();
        auto manifest = reader.read(*manifest_path);
        if (!manifest.has_value()) {
            return std::nullopt;
        }

        PackageResolver resolver(toolchain);
        auto resolved_packages = resolver.resolve(root, diagnostics);
        if (!resolved_packages.has_value()) {
            return std::nullopt;
        }

        auto* package = resolved_packages->find(manifest->name);
        if (package == nullptr) {
            return std::nullopt;
        }

        return Project(std::move(*resolved_packages), package, std::move(root));
    }

    static Project single_file(const std::filesystem::path& path,
                               const Toolchain& toolchain = Toolchain::discover()) {
        auto absolute_path = std::filesystem::absolute(path);
        auto source_directory = absolute_path.parent_path();
        PackageGraph packages;
        auto& root_package =
            packages.add(Manifest("standalone", "0.1.0", Manifest::Type::Kind::Executable, {}, {}),
                         source_directory, source_directory);

        if (!toolchain.standard_library.empty()) {
            ManifestReader reader;
            auto standard_manifest = reader.read(toolchain.standard_library / "zep.json");
            if (!standard_manifest.has_value()) {
                return Project(std::move(packages), &root_package, std::move(source_directory));
            }

            auto& standard_package =
                packages.add(std::move(*standard_manifest), toolchain.standard_library,
                             toolchain.standard_library / "src");
            root_package.dependencies.push_back(&standard_package);
        }

        return Project(std::move(packages), &root_package, std::move(source_directory));
    }
};
