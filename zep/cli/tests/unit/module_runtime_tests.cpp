#include <filesystem>
#include <gtest/gtest.h>

import zep.test.support;
import zep.cli.test.support;

TEST(CliZep, InitializesGlobalOwnersInDependencyOrderAndDestroysInReverse) {
    CliTestHarness harness("cli_global_startup", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "global_startup");
    harness.workspace.write("src/owner.zep", R"zep(
import std.text.String
extern fn putchar(value: i32) -> i32
public struct Owner {
    public:
        var text: String
        var value: i32
        fn Owner(value: i32) -> Owner {
            putchar(value)
            return Owner { text: String("owned"), value: value }
        }
        fn ~Owner() -> void { putchar(value + 32) }
}
var first = Owner(65)
public fn ready() -> boolean { return first.text.equals("owned") }
)zep");
    harness.workspace.write("src/main.zep", R"zep(
import owner.Owner
import owner.ready
import std.text.String
import std.core.error.Error
extern fn putchar(value: i32) -> i32
struct Failure : Error {}
@throws(Failure) fn unavailable() -> i32 { throw Failure {} }
var second = Owner(66)
var third = Owner(67)
var fallback = unavailable() catch { error: Failure -> 42, }
var names = [String("one"), String("two")]
public fn main() -> i32 {
    if (!ready() || !second.text.equals("owned") || !third.text.equals("owned")) { return 1 }
    if (fallback != 42 || !names[0].equals("one") || !names[1].equals("two")) { return 2 }
    putchar(77)
    return 0
}
)zep");

    const auto result = build_and_run(harness, "global_startup", "2");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
    EXPECT_EQ(result.stdout_text, "ABCMcba");
}

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

    const auto result = build_and_run(harness, "module_globals", "2");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliZep, RunsImportedFunctionValueFixture) {
    CliTestHarness harness("cli_zep_imported_function_value", ZEP_ZEP_EXECUTABLE,
                           "fixtures/function_value_import");
    const auto directory = harness.workspace.file("fixtures/function_value_import");
    const auto build = harness.run({"build"}, directory);

    harness.assert_success_and_exists("fixtures/function_value_import/build/function_value_import", build);
    ASSERT_TRUE(build.succeeded()) << build.stderr_text;

    const auto result = TestProcessRunner::run(
        {harness.workspace.file("fixtures/function_value_import/build/function_value_import").string()},
        directory);

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

    const auto result = build_and_run(harness, "generic_field_cleanup");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
    EXPECT_EQ(result.stdout_text, "C");
    EXPECT_TRUE(result.stderr_text.empty());
}
