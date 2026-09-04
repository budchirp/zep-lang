#include <filesystem>
#include <gtest/gtest.h>
#include <string>

import zep.test.support;
import zep.cli.test.support;

namespace {

void write_target_attribute_fixture(CliTestHarness& harness) {
    write_project_config(harness, "target_attribute");
    harness.workspace.write("src/target.zep", R"zep(@os("linux")
fn platform_code() -> i32 {
    return 1
}

@arch("x86_64")
fn architecture_code() -> i32 {
    return 2
}

public fn target_score() -> i32 {
    return platform_code() + architecture_code()
}
)zep");
    harness.workspace.write("src/main.zep", R"zep(import target.target_score

public fn main() -> i32 {
    if (target_score() != 3) {
        return 1
    }

    return 0
}
)zep");
}

void write_std_owned_bytes_fixture(CliTestHarness& harness) {
    write_project_config(harness, "std_owned_bytes");
    harness.workspace.write("src/bytes.zep", R"zep(import std.memory.Memory

public struct OwnedBytes {
    private:
        var ptr: *mut void
        var length: i32

    public:
        fn OwnedBytes(length: i32) -> OwnedBytes {
            var ptr = Memory::allocate_bytes(length as i64)
            return OwnedBytes { ptr: ptr, length: length }
        }

        fn length() -> i32 {
            return length
        }

        fn ~OwnedBytes() -> void {
            Memory::free(ptr)
        }
}
)zep");
    harness.workspace.write("src/main.zep", R"zep(import bytes.OwnedBytes

fn scoped_size() -> i32 {
    var bytes = OwnedBytes(32)
    return bytes.length()
}

public fn main() -> i32 {
    if (scoped_size() != 32) {
        return 1
    }

    var bytes = OwnedBytes(1)
    if (bytes.length() != 1) {
        return 2
    }

    return 0
}
)zep");
}

std::string containers_imports_source() {
    return R"zep(import std.memory.Memory

)zep";
}

std::string containers_memory_check_source() {
    return R"zep(fn check_memory() -> boolean {
    var mut raw = Memory::allocate<i32>()
    raw[0] = 44
    var result = raw[0] == 44
    Memory::free(raw as *mut void)

    return result
}

)zep";
}

std::string containers_main_source() {
    return R"zep(public fn main() -> i32 {
    if (!check_memory()) {
        return 1
    }

    return 0
}
)zep";
}

void write_stdlib_allocator_fixture(CliTestHarness& harness) {
    write_project_config(harness, "stdlib_allocator");
    harness.workspace.write("src/main.zep", containers_imports_source() +
                                                containers_memory_check_source() +
                                                containers_main_source());
}

std::string log_level_source() {
    return R"zep(public enum Level {
    Info
    Warning
    Error
}

)zep";
}

std::string log_entry_source() {
    return R"zep(public struct LogEntry {
    public:
        var level: Level
        var message: cstr

        fn LogEntry(level: Level, message: cstr) -> LogEntry {
            return LogEntry { level: level, message: message }
        }
}

)zep";
}

std::string log_counter_source() {
    return R"zep(public struct Counter {
    public:
        var info: i32
        var warning: i32
        var error: i32

        fn Counter() -> Counter {
            return Counter { info: 0, warning: 0, error: 0 }
        }

        fn add(level: Level) mut -> void {
            info = info + 1
        }

        fn total() -> i32 {
            return info + warning + error
        }
}

)zep";
}

std::string log_main_source() {
    return R"zep(import log.Level
import log.LogEntry
import log.Counter

public fn main() -> i32 {
    var mut counter = Counter()
    counter.add(Level::Info)
    counter.add(Level::Warning)
    counter.add(Level::Error)
    if (counter.info != 3 || counter.warning != 0 || counter.error != 0) {
        return 1
    }

    if (counter.total() != 3) {
        return 2
    }

    return 0
}
)zep";
}

void write_log_counter_runtime_fixture(CliTestHarness& harness) {
    write_project_config(harness, "log_counter_runtime");
    harness.workspace.write("src/log.zep",
                            log_level_source() + log_entry_source() + log_counter_source());
    harness.workspace.write("src/main.zep", log_main_source());
}

void write_stdlib_path_fixture(CliTestHarness& harness) {
    write_project_config(harness, "stdlib_path");
    harness.workspace.write("src/main.zep", R"zep(import std.fs.Path

public fn main() -> i32 {
    var parent = Path::dirname("/tmp/file.zep")
    if (!parent.equals("/tmp")) {
        return 1
    }

    var root = Path::dirname("/file.zep")
    if (!root.equals("/")) {
        return 2
    }

    var dot = Path::dirname("file.zep")
    if (!dot.equals(".")) {
        return 3
    }

    return 0
}
)zep");
}

} // namespace

TEST(CliZep, RunsImportedFunctionValueFixture) {
    CliTestHarness harness("cli_zep_imported_function_value", ZEP_ZEP_EXECUTABLE,
                           "fixtures/function_value_import");
    auto cwd = harness.workspace.file("fixtures/function_value_import");

    auto build = harness.run({"build"}, cwd);

    harness.assert_success_and_exists("fixtures/function_value_import/build/function_value_import",
                                      build);

    auto executable =
        harness.workspace.file("fixtures/function_value_import/build/function_value_import");
    auto result = TestProcessRunner::run({executable.string()}, cwd);

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliZep, RunsTargetAttributeFixture) {
    CliTestHarness harness("cli_zep_target_attribute", ZEP_ZEP_EXECUTABLE);
    write_target_attribute_fixture(harness);

    auto result = build_and_run(harness, "target_attribute");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliZep, RunsStdOwnedBytesFixture) {
    CliTestHarness harness("cli_zep_std_owned_bytes", ZEP_ZEP_EXECUTABLE);
    write_std_owned_bytes_fixture(harness);

    auto result = build_and_run(harness, "std_owned_bytes");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliZep, RunsStdlibAllocatorFixture) {
    CliTestHarness harness("cli_zep_stdlib_allocator", ZEP_ZEP_EXECUTABLE);
    write_stdlib_allocator_fixture(harness);

    auto result = build_and_run(harness, "stdlib_allocator");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliZep, RunsLogCounterRuntimeFixture) {
    CliTestHarness harness("cli_zep_log_counter_runtime", ZEP_ZEP_EXECUTABLE);
    write_log_counter_runtime_fixture(harness);

    auto result = build_and_run(harness, "log_counter_runtime");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliZep, RunsStdlibPathFixture) {
    CliTestHarness harness("cli_zep_stdlib_path", ZEP_ZEP_EXECUTABLE);
    write_stdlib_path_fixture(harness);

    auto result = build_and_run(harness, "stdlib_path");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}
