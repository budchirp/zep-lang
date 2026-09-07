module;

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

export module zep.workspace.manifest;

import zep.common.target;

export class PackageSource {
  public:
    enum class Type : std::uint8_t { Workspace, Path, Git, Global };
};

export class DependencySpec {
  public:
    std::string version;
    PackageSource::Type source;
    std::string location;

    DependencySpec(std::string version, PackageSource::Type source, std::string location = "")
        : version(std::move(version)), source(source), location(std::move(location)) {}
};

export class Manifest {
  public:
    class Type {
      public:
        enum class Kind : std::uint8_t {
            Executable,
            Library,
        };
    };

    class Target {
      public:
        std::string triple;
        std::vector<std::string> linker_arguments;
        Target(std::string triple, std::vector<std::string> linker_arguments)
            : triple(std::move(triple)), linker_arguments(std::move(linker_arguments)) {}
    };

    std::string name;
    std::string version;
    Type::Kind type;
    std::map<std::string, DependencySpec> libs;
    std::vector<Target> targets;

    Manifest(std::string name, std::string version, Type::Kind type,
             std::map<std::string, DependencySpec> libs, std::vector<Target> targets)
        : name(std::move(name)), version(std::move(version)), type(type), libs(std::move(libs)),
          targets(std::move(targets)) {}
};

export class ManifestReader {
  private:
    static std::string read_string(const nlohmann::json& object, const std::string& key) {
        if (!object.contains(key) || !object[key].is_string()) {
            throw std::runtime_error("manifest field '" + key + "' must be a string");
        }
        return object[key].get<std::string>();
    }

    static DependencySpec read_dependency(const std::string& name, const nlohmann::json& value) {
        if (value.is_string()) {
            return DependencySpec(value.get<std::string>(), PackageSource::Type::Global, "");
        }
        if (!value.is_object()) {
            throw std::runtime_error("dependency '" + name + "' must be a string or object");
        }

        auto version = read_string(value, "version");
        if (value.contains("path")) {
            return DependencySpec(version, PackageSource::Type::Path, read_string(value, "path"));
        }
        if (value.contains("git")) {
            return DependencySpec(version, PackageSource::Type::Git, read_string(value, "git"));
        }
        throw std::runtime_error("dependency '" + name + "' object requires 'path' or 'git'");
    }

    static std::vector<std::string> read_linker(const nlohmann::json& value) {
        if (!value.is_object()) {
            throw std::runtime_error("linker must be an object");
        }
        if (value.contains("flags")) {
            throw std::runtime_error(
                "legacy linker field 'flags' is not supported; use 'arguments'");
        }
        if (!value.contains("arguments")) {
            return {};
        }
        if (!value["arguments"].is_array()) {
            throw std::runtime_error("linker arguments must be an array of strings");
        }
        std::vector<std::string> arguments;
        const auto& values = value["arguments"];
        arguments.reserve(values.size());
        for (const auto& argument : values) {
            if (!argument.is_string()) {
                throw std::runtime_error("linker arguments must be an array of strings");
            }
            arguments.push_back(argument.get<std::string>());
        }
        return arguments;
    }

    static Manifest::Target read_target(const nlohmann::json& value) {
        if (!value.is_object()) {
            throw std::runtime_error("target entries must be objects");
        }

        auto linker =
            value.contains("linker") ? read_linker(value["linker"]) : std::vector<std::string>();

        if (value.contains("triple")) {
            auto triple = read_string(value, "triple");
            if (triple.empty()) {
                throw std::runtime_error("target triple cannot be empty");
            }

            auto resolved_triple = triple == "host" ? TargetInfo::host_triple() : std::move(triple);
            return Manifest::Target(std::move(resolved_triple), std::move(linker));
        }

        auto architecture = TargetArch::Kind::Type::Unknown;
        if (value.contains("arch")) {
            architecture = TargetArch::from(read_string(value, "arch"));
        }

        auto operating_system = TargetOS::Kind::Type::Unknown;
        if (value.contains("os")) {
            operating_system = TargetOS::from(read_string(value, "os"));
        }

        if (architecture == TargetArch::Kind::Type::Unknown &&
            operating_system == TargetOS::Kind::Type::Unknown) {
            return Manifest::Target(TargetInfo::host_triple(), std::move(linker));
        }

        auto triple = TargetInfo::triple_from(architecture, operating_system);
        if (triple.empty()) {
            throw std::runtime_error("target object expects valid 'os' and 'arch'");
        }

        return Manifest::Target(std::move(triple), std::move(linker));
    }

  public:
    std::optional<std::filesystem::path> find(const std::filesystem::path& start) const {
        auto current = std::filesystem::absolute(start);
        if (!std::filesystem::is_directory(current)) {
            current = current.parent_path();
        }
        while (!current.empty()) {
            auto manifest = current / "zep.json";
            if (std::filesystem::is_regular_file(manifest)) {
                return manifest;
            }
            auto parent = current.parent_path();
            if (parent == current) {
                break;
            }
            current = std::move(parent);
        }
        return std::nullopt;
    }

    std::optional<Manifest> read(const std::filesystem::path& manifest_path) const {
        std::ifstream file(manifest_path);
        if (!file.is_open()) {
            return std::nullopt;
        }
        std::ostringstream content;
        content << file.rdbuf();
        try {
            auto root = nlohmann::json::parse(content.str());
            if (!root.is_object()) {
                throw std::runtime_error("manifest root must be an object");
            }
            auto package_root = manifest_path.parent_path();
            std::string name = package_root.filename().string();
            if (root.contains("name")) {
                name = read_string(root, "name");
            }
            if (name.empty()) {
                throw std::runtime_error("manifest name cannot be empty");
            }
            std::string version = root.contains("version") ? read_string(root, "version") : "0.1.0";
            auto type = Manifest::Type::Kind::Executable;
            if (root.contains("type")) {
                auto type_name = read_string(root, "type");
                if (type_name == "library") {
                    type = Manifest::Type::Kind::Library;
                } else if (type_name != "executable") {
                    throw std::runtime_error("manifest type must be 'executable' or 'library'");
                }
            }

            std::map<std::string, DependencySpec> libs;
            if (root.contains("libs")) {
                if (!root["libs"].is_object()) {
                    throw std::runtime_error("libs must be an object");
                }
                for (const auto& [dependency_name, dependency] : root["libs"].items()) {
                    libs.emplace(dependency_name, read_dependency(dependency_name, dependency));
                }
            }

            std::vector<Manifest::Target> targets;
            if (root.contains("target")) {
                if (!root["target"].is_array()) {
                    throw std::runtime_error("target must be an array of objects");
                }
                const auto& target_values = root["target"];
                targets.reserve(target_values.size());
                for (const auto& target : target_values) {
                    targets.push_back(read_target(target));
                }
            }
            if (targets.empty()) {
                targets.emplace_back(TargetInfo::host_triple(), std::vector<std::string>());
            }
            return Manifest(std::move(name), std::move(version), type, std::move(libs),
                            std::move(targets));
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
};
