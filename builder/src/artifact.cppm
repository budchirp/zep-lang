module;

#include <filesystem>
#include <utility>

export module zep.build.artifact;

export class ObjectArtifact {
  public:
    std::filesystem::path path;

    explicit ObjectArtifact(std::filesystem::path path) : path(std::move(path)) {}
};

export class ExecutableArtifact {
  public:
    std::filesystem::path path;

    explicit ExecutableArtifact(std::filesystem::path path) : path(std::move(path)) {}
};
