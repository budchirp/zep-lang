module;

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <gtest/gtest.h>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

export module zep.test.support;

import zep.common.system.command;
import zep.common.system.posix;
import zep.common.system.process;

export using ::ProcessResult;

export class TestPaths {
  private:
    static std::filesystem::path env_path(const char* name) {
        auto value = std::getenv(name);
        if (value == nullptr) {
            return std::filesystem::current_path();
        }

        return std::filesystem::path(value);
    }

  public:
    static std::filesystem::path source_root() { return env_path("ZEP_TEST_SOURCE_DIR"); }

    static std::filesystem::path binary_root() { return env_path("ZEP_TEST_BINARY_DIR"); }

    static std::filesystem::path project_source_root() {
        return env_path("ZEP_PROJECT_SOURCE_DIR");
    }

    static std::filesystem::path project_binary_root() {
        return env_path("ZEP_PROJECT_BINARY_DIR");
    }
};

export class TextFile {
  public:
    static std::string read(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("could not read file: " + path.string());
        }

        std::ostringstream stream;
        stream << file.rdbuf();
        return stream.str();
    }

    static void write(const std::filesystem::path& path, const std::string& text) {
        auto parent = path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("could not write file: " + path.string());
        }

        file << text;
    }
};

export class TestWorkspace {
  private:
    std::filesystem::path path;

    static std::filesystem::path make_root(const std::string& name) {
        auto safe_name = name;
        std::ranges::replace(safe_name, '/', '_');
        std::ranges::replace(safe_name, '\\', '_');

        auto root = TestPaths::binary_root() / "tmp" / safe_name;
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        return root;
    }

  public:
    explicit TestWorkspace(std::string name) : path(make_root(name)) {}

    ~TestWorkspace() { std::filesystem::remove_all(path); }

    TestWorkspace(const TestWorkspace&) = delete;
    TestWorkspace& operator=(const TestWorkspace&) = delete;

    const std::filesystem::path& root() const { return path; }

    std::filesystem::path file(const std::filesystem::path& relative) const {
        return path / relative;
    }

    void write(const std::filesystem::path& relative, const std::string& text) const {
        TextFile::write(file(relative), text);
    }

    void copy_fixture(const std::filesystem::path& relative) const {
        auto source = TestPaths::source_root() / relative;
        auto destination = file(relative);

        if (std::filesystem::is_directory(source)) {
            std::filesystem::create_directories(destination);

            for (auto iterator = std::filesystem::recursive_directory_iterator(source);
                 iterator != std::filesystem::recursive_directory_iterator(); ++iterator) {
                if (iterator->is_directory() && iterator->path().filename() == "build") {
                    iterator.disable_recursion_pending();
                    continue;
                }

                auto target = destination / std::filesystem::relative(iterator->path(), source);
                if (iterator->is_directory()) {
                    std::filesystem::create_directories(target);
                } else {
                    std::filesystem::copy_file(iterator->path(), target,
                                               std::filesystem::copy_options::overwrite_existing);
                }
            }

            return;
        }

        std::filesystem::create_directories(destination.parent_path());
        std::filesystem::copy_file(source, destination,
                                   std::filesystem::copy_options::overwrite_existing);
    }
};

export class TestProcessRunner {
  public:
    static ProcessResult run(const std::vector<std::string>& arguments,
                             const std::filesystem::path& working_directory) {
        PosixProcessRunner process_runner;
        return process_runner.run(Command(arguments, working_directory));
    }
};

export std::string capture_stdout(const std::function<void()>& fn) {
    testing::internal::CaptureStdout();
    fn();
    return testing::internal::GetCapturedStdout();
}

export std::string capture_stderr(const std::function<void()>& fn) {
    testing::internal::CaptureStderr();
    fn();
    return testing::internal::GetCapturedStderr();
}

export class CliTestHarness {
  private:
    std::string executable;

  public:
    TestWorkspace workspace;

    CliTestHarness(const std::string& name, const std::string& exec,
                   const std::string& fixture = "")
        : executable(exec), workspace(name) {
        if (!fixture.empty()) {
            workspace.copy_fixture(fixture);
        }
    }

    ProcessResult run(const std::vector<std::string>& args, const std::filesystem::path& cwd = "") {
        auto actual_cwd = cwd.empty() ? workspace.root() : cwd;
        std::vector<std::string> full_args = {executable};
        full_args.insert(full_args.end(), args.begin(), args.end());
        return TestProcessRunner::run(full_args, actual_cwd);
    }

    void assert_success_and_exists(const std::filesystem::path& output,
                                   const ProcessResult& result) {
        EXPECT_TRUE(result.succeeded()) << result.stderr_text << result.stdout_text;
        EXPECT_TRUE(std::filesystem::exists(workspace.file(output)));
    }

    void assert_fails_with_stderr(const std::string& text, const ProcessResult& result) {
        EXPECT_FALSE(result.succeeded());
        EXPECT_NE(result.stderr_text.find(text), std::string::npos);
    }
};

export class Snapshot {
  private:
    static void replace_all(std::string& text, const std::string& from, const std::string& to) {
        if (from.empty()) {
            return;
        }

        std::size_t position = 0;
        while ((position = text.find(from, position)) != std::string::npos) {
            text.replace(position, from.size(), to);
            position += to.size();
        }
    }

    static bool is_volatile_line(const std::string& line) {
        return line.starts_with("target datalayout =") || line.starts_with("target triple =");
    }

    static std::string strip_volatile_lines(const std::string& text) {
        std::istringstream input(text);
        std::ostringstream output;
        std::string line;
        auto first = true;

        while (std::getline(input, line)) {
            if (is_volatile_line(line)) {
                continue;
            }

            if (!first) {
                output << '\n';
            }
            first = false;
            output << line;
        }

        if (!text.empty() && text.back() == '\n') {
            output << '\n';
        }

        return output.str();
    }

  public:
    static std::string normalize(std::string text) {
        replace_all(text, "\r\n", "\n");
        replace_all(text, TestPaths::source_root().string(), "<TEST_SOURCE_DIR>");
        replace_all(text, TestPaths::binary_root().string(), "<TEST_BINARY_DIR>");
        replace_all(text, TestPaths::project_source_root().string(), "<PROJECT_SOURCE_DIR>");
        replace_all(text, TestPaths::project_binary_root().string(), "<PROJECT_BINARY_DIR>");
        return strip_volatile_lines(text);
    }

    static bool matches(const std::filesystem::path& relative, const std::string& actual) {
        auto path = TestPaths::source_root() / relative;
        auto normalized = normalize(actual);

        auto update = std::getenv("ZEP_UPDATE_SNAPSHOTS");
        if (update != nullptr && std::string(update) == "1") {
            TextFile::write(path, normalized);
            return true;
        }

        if (!std::filesystem::exists(path)) {
            return false;
        }

        return normalize(TextFile::read(path)) == normalized;
    }
};
