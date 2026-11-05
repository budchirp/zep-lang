module;

#include <filesystem>
#include <system_error>

export module zep.common.paths;

export class Paths {
  public:
    static std::filesystem::path absolute(const std::filesystem::path& path) {
        return std::filesystem::absolute(path).lexically_normal();
    }

    static std::filesystem::path canonical(const std::filesystem::path& path,
                                           std::error_code& error) {
        return std::filesystem::weakly_canonical(path, error);
    }

    static std::filesystem::path canonical(const std::filesystem::path& path) {
        std::error_code error;
        auto result = canonical(path, error);

        if (error) {
            throw std::filesystem::filesystem_error("could not canonicalize path", path, error);
        }

        return result;
    }

    static std::filesystem::path identity(const std::filesystem::path& path) {
        std::error_code error;
        auto result = canonical(path, error);

        return error ? absolute(path) : result;
    }
};
