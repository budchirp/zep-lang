module;

#include <filesystem>
#include <string>
#include <vector>

export module zep.build.fetch;

import zep.common.logger;
import zep.common.system.command;
import zep.common.system.process;
import zep.workspace.manifest;

export class DependencyFetcher {
  private:
    ProcessRunner& process_runner;

    bool run(const std::vector<std::string>& arguments,
             const std::filesystem::path& working_directory) const {
        const auto result = process_runner.run(Command(arguments, working_directory));

        if (!result.succeeded()) {
            Logger::print_stderr("zep: error: ", result.stderr_text, "\n");

            return false;
        }

        return true;
    }

  public:
    explicit DependencyFetcher(ProcessRunner& process_runner) : process_runner(process_runner) {}

    bool fetch(const std::filesystem::path& path) const {
        ManifestReader reader;
        const auto manifest_path = reader.find(path);

        if (!manifest_path.has_value()) {
            return Logger::fail("could not find zep.json");
        }

        const auto manifest = reader.read(*manifest_path);

        if (!manifest.has_value()) {
            return Logger::fail("could not read zep.json");
        }

        const auto root = manifest_path->parent_path();

        for (const auto& [name, dependency] : manifest->libs) {
            if (dependency.source != PackageSource::Type::Git) {
                continue;
            }

            const auto target = root / "build/libs" / name / dependency.version;
            std::filesystem::create_directories(target.parent_path());

            if (!std::filesystem::exists(target)) {
                if (!run({"git", "clone", dependency.location, target.string()}, root)) {
                    return false;
                }
            } else if (!run({"git", "-C", target.string(), "fetch", "origin"}, root) ||
                       !run({"git", "-C", target.string(), "pull", "--ff-only"}, root)) {
                return false;
            }

            Logger::print("fetched ", name, " -> ", target.string(), "\n");
        }

        return true;
    }
};
