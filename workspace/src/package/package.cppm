module;

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

export module zep.workspace.package;

import zep.workspace.manifest;

export class Package {
  public:
    Manifest manifest;
    std::filesystem::path root;
    std::filesystem::path source_directory;
    std::vector<Package*> dependencies;

    Package(Manifest manifest, std::filesystem::path root, std::filesystem::path source_directory)
        : manifest(std::move(manifest)), root(std::move(root)),
          source_directory(std::move(source_directory)) {}
};
