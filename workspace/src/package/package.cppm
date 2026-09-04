module;

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

export module zep.workspace.package;

import zep.workspace.manifest;

export class Package {
  public:
    Manifest manifest;
    PackageSource::Type source;
    std::filesystem::path root;
    std::vector<Package*> dependencies;

    Package(Manifest manifest, PackageSource::Type source, std::filesystem::path root)
        : manifest(std::move(manifest)), source(source), root(std::move(root)) {}
};
