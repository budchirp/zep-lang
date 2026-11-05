module;

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

export module zep.build.linker;

import zep.common.logger;
import zep.common.system.command;
import zep.common.system.process;
import zep.common.target;

export class Linker {
  private:
    std::filesystem::path compiler;
    ProcessRunner& process_runner;

  public:
    Linker(std::filesystem::path compiler, ProcessRunner& process_runner)
        : compiler(std::move(compiler)), process_runner(process_runner) {}

    bool link(const std::vector<std::filesystem::path>& objects,
              const std::filesystem::path& output, const TargetInfo& target,
              const std::vector<std::string>& linker_arguments,
              const std::filesystem::path& working_directory) {
        std::vector<std::string> arguments;
        arguments.reserve(objects.size() + linker_arguments.size() + 7);
        arguments.push_back(compiler.string());
        arguments.push_back("-fuse-ld=lld");
        arguments.push_back("--target=" + target.triple);

        for (const auto& object : objects) {
            arguments.push_back(object.string());
        }

        arguments.insert(arguments.end(), linker_arguments.begin(), linker_arguments.end());
        arguments.push_back("-Wl,--allow-multiple-definition");
        arguments.push_back("-o");
        arguments.push_back(output.string());

        std::filesystem::create_directories(output.parent_path());

        const auto result = process_runner.run(Command(std::move(arguments), working_directory));

        if (!result.succeeded()) {
            Logger::print_stderr("zep: error: ", result.stderr_text, "\n");

            return false;
        }

        return true;
    }
};
