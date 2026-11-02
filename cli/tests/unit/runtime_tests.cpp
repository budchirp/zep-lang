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

void write_stdlib_format_fixture(CliTestHarness& harness) {
    write_project_config(harness, "stdlib_format");
    harness.workspace.write("src/main.zep", R"zep(import std.io.format.FormatArguments
import std.io.format.sprint
import std.io.eprint
import std.io.print
import std.text.string.String

public fn main() -> i32 {
    var mut arguments = FormatArguments()
    var text = String("text")

    arguments.add(&text)
    arguments.add(-42)
    arguments.add(12 as u64)
    arguments.add(3.5)
    arguments.add(true)
    arguments.add('!')

    var formatted = sprint("{} {} {} {} {} {} {{ok}}", &arguments)
    if (formatted.is_error()) {
        return 1
    }

    if (!formatted.unwrap().equals("text -42 12 3.5 true ! {ok}")) {
        return 2
    }

    var empty_arguments = FormatArguments()
    var missing_argument = sprint("{}", &empty_arguments)
    if (!missing_argument.is_error()) {
        return 3
    }

    var missing_argument_error = missing_argument.unwrap_error()
    if (!missing_argument_error.message().equals("missing format argument at index 0")) {
        return 4
    }

    arguments.clear()
    arguments.add(1)
    var extra_argument = sprint("plain", &arguments)
    if (!extra_argument.is_error()) {
        return 5
    }

    var extra_argument_error = extra_argument.unwrap_error()
    if (!extra_argument_error.message().equals("unused format argument at index 0")) {
        return 6
    }

    var unmatched_opening = sprint("{", &empty_arguments)
    if (!unmatched_opening.is_error()) {
        return 7
    }

    var unmatched_opening_error = unmatched_opening.unwrap_error()
    if (!unmatched_opening_error.message().equals("unmatched opening brace at index 0")) {
        return 8
    }

    var unmatched_closing = sprint("}", &empty_arguments)
    if (!unmatched_closing.is_error()) {
        return 9
    }

    var unmatched_closing_error = unmatched_closing.unwrap_error()
    if (!unmatched_closing_error.message().equals("unmatched closing brace at index 0")) {
        return 10
    }

    var literal = sprint("literal")
    if (literal.is_error() || !literal.unwrap().equals("literal")) {
        return 11
    }

    arguments.clear()
    arguments.add(7)
    print("stdout {}", &arguments)

    arguments.clear()
    arguments.add(false)
    eprint("stderr {}", &arguments)

    return 0
}
)zep");
}

} // namespace

TEST(CliZep, PreservesPrivateModuleGlobalsAcrossImportedCalls) {
    CliTestHarness harness("cli_module_globals", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "module_globals");
    harness.workspace.write("src/counter.zep", R"zep(
public fn read_count() -> i32 { return count }
var mut count: i32 = 3 + 4
struct Pair { public: var first: i32 var second: i32 }
var initial = Pair { second: 4, first: 1 + 2 }
var marker: char = 'G'
var message: cstr = "globals"
var words: cstr[2] = ["first", "second"]
extern fn strcmp(first: cstr, second: cstr) -> i32

public fn increment() -> void { count = count + initial.first }
public fn valid_scalars() -> boolean {
    return marker == 'G' && strcmp(message, "globals") == 0 &&
        strcmp(words[1], "second") == 0
}
)zep");
    harness.workspace.write("src/other.zep", R"zep(
var mut count: i32 = 20
public fn other_count() -> i32 { count = count + 1 return count }
)zep");
    harness.workspace.write("src/main.zep", R"zep(
import counter.increment
import counter.read_count
import counter.valid_scalars
import other.other_count
public fn main() -> i32 {
    if (read_count() != 7) { return 1 }
    increment()
    if (read_count() != 10) { return 2 }
    if (other_count() != 21) { return 3 }
    if (!valid_scalars()) { return 4 }
    return 0
}
)zep");

    for (const auto* level : {"0", "2"}) {
        auto build = harness.run({"build", "-O", level}, harness.workspace.root());
        ASSERT_TRUE(build.succeeded()) << build.stderr_text;
        auto result = TestProcessRunner::run(
            {harness.workspace.file("build/module_globals").string()}, harness.workspace.root());
        EXPECT_TRUE(result.succeeded()) << result.stderr_text << "exit: " << result.exit.status;
    }
}

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

TEST(CliZep, RunsStdlibFormattingFixture) {
    CliTestHarness harness("cli_zep_stdlib_format", ZEP_ZEP_EXECUTABLE);
    write_stdlib_format_fixture(harness);

    auto result = build_and_run(harness, "stdlib_format");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text << "exit status: " << result.exit.status;
    EXPECT_EQ(result.stdout_text, "stdout 7");
    EXPECT_EQ(result.stderr_text, "stderr false");
}

TEST(CliZep, RunsStdlibPanicFixture) {
    CliTestHarness harness("cli_zep_stdlib_panic", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "stdlib_panic");
    harness.workspace.write("src/main.zep", R"zep(import std.io.panic.panic

public fn main() -> i32 {
    panic("fatal error")
}
)zep");

    auto build = harness.run({"build"});
    harness.assert_success_and_exists("build/stdlib_panic", build);

    auto executable = harness.workspace.file("build/stdlib_panic");
    auto result = TestProcessRunner::run({executable.string()}, harness.workspace.root());

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.exit.status, 1);
    EXPECT_EQ(result.stdout_text, "");
    EXPECT_EQ(result.stderr_text, "fatal error\n");
}

TEST(CliZep, PreservesFormatArgumentsGrowthBoundsOverloadsAndReuse) {
    CliTestHarness harness("cli_zep_format_arguments", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "format_arguments");
    harness.workspace.write("src/main.zep",
                            TextFile::read(TestPaths::source_root() /
                                           "fixtures/format_arguments.zep"));

    auto result = build_and_run(harness, "format_arguments");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text << "exit status: " << result.exit.status;
    EXPECT_TRUE(result.stdout_text.empty());
    EXPECT_TRUE(result.stderr_text.empty());
}

TEST(CliZep, CompilesStandardLibraryHttpModules) {
    CliTestHarness harness("cli_zep_http_modules", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "http_modules");
    harness.workspace.write("src/main.zep",
                            TextFile::read(TestPaths::source_root() / "fixtures/http_modules.zep"));

    auto result = build_and_run(harness, "http_modules");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliZep, DropsSpecializedGenericFieldsInNonGenericOwners) {
    CliTestHarness harness("cli_zep_generic_field_cleanup", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "generic_field_cleanup");
    harness.workspace.write("src/owner.zep", R"zep(
extern fn putchar(value: i32) -> i32

public struct Cell<T> {
    public:
        var value: T
        fn ~Cell() -> void { putchar(67) }
}

struct Owner {
    public:
        var cell: Cell<i32>
}

public fn exercise() -> void {
    var owner = Owner { cell: Cell<i32> { value: 7 } }
}
)zep");
    harness.workspace.write("src/main.zep", R"zep(
import owner.exercise

public fn main() -> i32 {
    exercise()
    return 0
}
)zep");

    auto result = build_and_run(harness, "generic_field_cleanup");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text << "exit status: " << result.exit.status;
    EXPECT_EQ(result.stdout_text, "C");
    EXPECT_TRUE(result.stderr_text.empty());
}
