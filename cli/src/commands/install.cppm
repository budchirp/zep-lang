module;

#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>

export module zep.cli.commands.install;

import argman;
import zep.build;
import zep.common.logger;

export class InstallCommand : public argman::Command {
  private:
    std::filesystem::path standard_source;

  public:
    explicit InstallCommand(std::filesystem::path standard_source)
        : standard_source(std::move(standard_source)) {}
    argman::Command::Info info() override {
        return {.name = "install",
                .description =
                    "Install the zep compiler and standard library to ~/.local/share/zep/"};
    }

    int execute() override {
        auto* home_value = std::getenv("HOME");
        if (home_value == nullptr) {
            Logger::print_stderr("could not determine HOME\n");
            return 1;
        }

        auto home = std::string(home_value);
        auto executable_path = std::filesystem::canonical("/proc/self/exe");
        return Installer::install(executable_path, standard_source,
                                  std::filesystem::path(home) / ".local/share/zep", ZEP_STD_VERSION)
                   ? 0
                   : 1;
    }
};
