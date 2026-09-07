module;

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

export module zep.workspace.package.graph;

import zep.workspace.package;
import zep.workspace.manifest;
import zep.workspace.toolchain;
import zep.common.diagnostic.collection;
import zep.common.source.position;
import zep.common.source;
import zep.common.source.span;

export class PackageGraph {
  private:
    std::map<std::string, std::unique_ptr<Package>> packages;

  public:
    Package& add(Manifest manifest, std::filesystem::path root,
                 std::filesystem::path source_directory) {
        auto name = manifest.name;
        auto package = std::make_unique<Package>(std::move(manifest), std::move(root),
                                                 std::move(source_directory));
        auto [iterator, inserted] = packages.emplace(name, std::move(package));
        if (!inserted) {
            throw std::invalid_argument("package already exists: " + name);
        }

        return *iterator->second;
    }

    Package* find(const std::string& name) const {
        auto iterator = packages.find(name);
        if (iterator == packages.end()) {
            return nullptr;
        }
        return iterator->second.get();
    }
};

export class PackageResolver {
  private:
    Toolchain environment;
    ManifestReader manifest_reader;
    std::set<std::string> resolving;
    std::vector<std::unique_ptr<Source>> sources;

    void report(Diagnostics& diagnostics, const std::filesystem::path& path, std::string message) {
        sources.push_back(std::make_unique<Source>(path.string(), std::string()));
        diagnostics.add_error(*sources.back(), Span::from_position(Position()), std::move(message));
    }

    std::optional<std::filesystem::path> locate(const DependencySpec& dependency,
                                                const std::string& name,
                                                const std::filesystem::path& workspace_root) const {
        if (dependency.source == PackageSource::Type::Path) {
            auto path = std::filesystem::path(dependency.location);
            if (path.is_relative()) {
                path = workspace_root / path;
            }
            return path;
        }
        if (dependency.source == PackageSource::Type::Git) {
            auto checkout = workspace_root / "build/libs" / name / dependency.version;
            return std::filesystem::is_directory(checkout) ? std::optional(checkout) : std::nullopt;
        }
        if (name == "std" && !environment.standard_library.empty() &&
            std::filesystem::is_regular_file(environment.standard_library / "zep.json")) {
            return environment.standard_library;
        }
        for (const auto& root : environment.package_roots) {
            if (!dependency.version.empty()) {
                auto versioned = root / name / dependency.version;
                if (std::filesystem::is_directory(versioned) &&
                    std::filesystem::is_regular_file(versioned / "zep.json")) {
                    return versioned;
                }
            }
            auto unversioned = root / name;
            if (std::filesystem::is_directory(unversioned) &&
                std::filesystem::is_regular_file(unversioned / "zep.json")) {
                return unversioned;
            }
        }
        return std::nullopt;
    }

    Package* resolve_package(PackageGraph& graph, const std::filesystem::path& root,
                             Diagnostics& diagnostics) {
        auto manifest_path = root / "zep.json";
        auto manifest = manifest_reader.read(manifest_path);
        if (!manifest.has_value()) {
            report(diagnostics, manifest_path, "could not read manifest");
            return nullptr;
        }
        auto name = manifest->name;
        if (resolving.contains(name)) {
            report(diagnostics, manifest_path, "package dependency cycle involving '" + name + "'");
            return nullptr;
        }
        if (auto* existing = graph.find(name); existing != nullptr) {
            return existing;
        }
        resolving.insert(name);
        auto& package = graph.add(std::move(*manifest), root, root / "src");
        for (const auto& [dependency_name, dependency] : package.manifest.libs) {
            auto dependency_root = locate(dependency, dependency_name, package.root);
            if (!dependency_root.has_value() || !std::filesystem::is_directory(*dependency_root)) {
                report(diagnostics, manifest_path,
                       "could not locate dependency '" + dependency_name + "'");
                continue;
            }
            auto* dependency_package = resolve_package(graph, *dependency_root, diagnostics);
            if (dependency_package != nullptr) {
                package.dependencies.push_back(dependency_package);
            }
        }
        resolving.erase(name);
        return &package;
    }

  public:
    explicit PackageResolver(Toolchain environment) : environment(std::move(environment)) {}

    PackageResolver() : PackageResolver(Toolchain::discover()) {}

    std::optional<PackageGraph> resolve(const std::filesystem::path& workspace_root,
                                        Diagnostics& diagnostics) {
        PackageGraph graph;
        auto root = std::filesystem::absolute(workspace_root);
        if (resolve_package(graph, root, diagnostics) == nullptr || diagnostics.has_errors()) {
            return std::nullopt;
        }
        return std::optional<PackageGraph>(std::move(graph));
    }
};
