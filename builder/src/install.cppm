module;

#include <filesystem>
#include <string>

export module zep.build.install;

import zep.common.logger;

export class Installer {
  public:
    static bool install(const std::filesystem::path& executable,
                        const std::filesystem::path& standard_source,
                        const std::filesystem::path& destination, const std::string& version) {
        const auto binary_directory = destination / "bin";
        const auto standard = destination / "libs/std" / version;

        std::filesystem::create_directories(binary_directory);
        std::filesystem::create_directories(standard.parent_path());
        std::filesystem::copy_file(executable, binary_directory / "zep",
                                   std::filesystem::copy_options::overwrite_existing);

        std::filesystem::remove_all(standard);
        std::filesystem::create_directories(standard);
        std::filesystem::copy_file(standard_source / "zep.json", standard / "zep.json",
                                   std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy(standard_source / "src", standard / "src",
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);

        Logger::print("installed zep to ", (binary_directory / "zep").string(), "\n");
        Logger::print("installed std to ", standard.string(), "\n");

        return true;
    }
};
