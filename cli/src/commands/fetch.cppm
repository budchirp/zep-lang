module;

#include <filesystem>
#include <string>

export module zep.cli.commands.fetch;

import argman;
import zep.common.system.process;
import zep.build;

export class FetchCommand : public argman::Command {
  private:
    ProcessRunner& process_runner;

  public:
    argman::Command::Info info() override {
        return {.name = "fetch",
                .description = "Fetch project dependencies",
                .options = {
                    argman::Option("project", "Project root directory", std::string("")),
                }};
    }

    explicit FetchCommand(ProcessRunner& process_runner) : process_runner(process_runner) {}

    int execute() override {
        auto project = get<std::string>("project");
        auto path =
            project.empty() ? std::filesystem::current_path() : std::filesystem::path(project);
        DependencyFetcher fetcher(process_runner);
        return fetcher.fetch(path) ? 0 : 1;
    }
};
