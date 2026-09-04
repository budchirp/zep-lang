module;

#include <filesystem>
#include <string>

export module zep.cli.commands.fetch;

import argman;
import zep.common.system.process;
import zep.build.service;
import zep.common.logger;
import zep.workspace.manifest;
import zep.workspace.toolchain;
import zep.workspace.manifest;

export class FetchCommand : public argman::Command {
  public:
    argman::Command::Info info() override {
        return {.name = "fetch",
                .description = "Fetch project dependencies",
                .options = {
                    argman::Option("project", "Project root directory", std::string("")),
                }};
    }

  private:
  public:
    explicit FetchCommand(ProcessRunner& process_runner) : process_runner(process_runner) {}

    int execute() override {
        auto project_root = get<std::string>("project");
        if (project_root.empty()) {
            auto discovered_manifest = ManifestReader().find(std::filesystem::current_path());
            if (!discovered_manifest.has_value()) {
                Logger::print_stderr("zep: error: could not find zep.json\n");
                return 1;
            }
            project_root = discovered_manifest->parent_path().string();
        }

        project_root = std::filesystem::weakly_canonical(project_root).string();

        auto config = ManifestReader().read(std::filesystem::path(project_root) / "zep.json");
        if (!config.has_value()) {
            return 1;
        }

        DependencyFetcher fetcher(process_runner, project_root);
        if (!fetcher.fetch(*config)) {
            return 1;
        }

        return 0;
    }

  private:
    ProcessRunner& process_runner;
};
