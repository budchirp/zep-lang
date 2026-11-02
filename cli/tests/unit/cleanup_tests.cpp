#include <filesystem>
#include <gtest/gtest.h>
#include <string>

import zep.test.support;
import zep.cli.test.support;

TEST(CliCleanup, DestroysImportedOwnersExplicitlyExactlyOnce) {
    CliTestHarness harness("cli_imported_explicit_drop", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "explicit_drop");
    harness.workspace.write("src/owner.zep", R"zep(
extern fn putchar(value: i32) -> i32
public struct Owner {
    public:
        var value: i32
        fn ~Owner() -> void { putchar(value) }
}
public struct Cell<T> {
    public:
        var value: T
        fn ~Cell() -> void { putchar(67) }
}
)zep");
    harness.workspace.write("src/main.zep", R"zep(
import owner.Owner
import owner.Cell
import std.io.format.FormatArguments
public fn main() -> i32 {
    var mut owner = Owner { value: 65 }
    owner.~Owner()
    var mut cell = Cell<i32> { value: 1 }
    cell.~Cell()
    var mut arguments = FormatArguments()
    arguments.add("owned")
    arguments.~FormatArguments()
    return 0
}
)zep");

    for (const auto* level : {"0", "2"}) {
        auto build = harness.run({"build", "-O", level}, harness.workspace.root());
        ASSERT_TRUE(build.succeeded()) << build.stderr_text;
        auto result = TestProcessRunner::run(
            {harness.workspace.file("build/explicit_drop").string()}, harness.workspace.root());
        EXPECT_TRUE(result.succeeded()) << result.stderr_text;
        EXPECT_EQ(result.stdout_text, "AC");
    }
}

TEST(CliCleanup, DropsDiscardedOwningCallResults) {
    CliTestHarness harness("cli_discarded_owner", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "discarded_owner");
    harness.workspace.write("src/main.zep", TextFile::read(
        TestPaths::source_root() / "fixtures/discarded_calls.zep"));

    for (const auto* level : {"0", "2"}) {
        auto build = harness.run({"build", "-O", level}, harness.workspace.root());
        ASSERT_TRUE(build.succeeded()) << build.stderr_text;
        auto result = TestProcessRunner::run(
            {harness.workspace.file("build/discarded_owner").string()}, harness.workspace.root());
        EXPECT_TRUE(result.succeeded()) << result.stderr_text;
        EXPECT_EQ(result.stdout_text, "ACDEB");
    }
}
