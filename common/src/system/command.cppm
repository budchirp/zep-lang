module;

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

export module zep.common.system.command;

export class Command {
  public:
    std::vector<std::string> arguments;
    std::filesystem::path working_directory;

    explicit Command(std::vector<std::string> arguments,
                     std::filesystem::path working_directory = {})
        : arguments(std::move(arguments)), working_directory(std::move(working_directory)) {}
};
