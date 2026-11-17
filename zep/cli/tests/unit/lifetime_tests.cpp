#include <gtest/gtest.h>

import zep.test.support;
import zep.cli.test.support;

TEST(CliCleanup, SharesEnumFieldsAndErasesVoidPayloads) {
    CliTestHarness harness("cli_enums", ZEP_ZEP_EXECUTABLE);
    write_test_group(harness, TestPaths::project_source_root() / "cli/tests/fixtures/enums");

    for (const auto* level : {"0", "2"}) {
        SCOPED_TRACE(level);
        const auto result = harness.run({"test", "-O", level});

        ASSERT_TRUE(result.succeeded()) << result.stderr_text << "exit: " << result.exit.status;
    }
}

TEST(CliCleanup, DestroysImportedOwnersExplicitlyExactlyOnce) {
    CliTestHarness harness("cli_imported_explicit_destruction", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "explicit_destruction");
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

    const auto result = build_and_run(harness, "explicit_destruction", "2");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
    EXPECT_EQ(result.stdout_text, "AC");
}

TEST(CliCleanup, ConstructsCopiesAndRebuildsOwnedStorage) {
    CliTestHarness harness("cli_lifetime", ZEP_ZEP_EXECUTABLE);
    write_test_group(harness, TestPaths::project_source_root() / "cli/tests/fixtures/lifetime");

    for (const auto* level : {"0", "2"}) {
        SCOPED_TRACE(level);
        const auto result = harness.run({"test", "-O", level});

        ASSERT_TRUE(result.succeeded()) << result.stderr_text << "exit: " << result.exit.status;
    }
}

TEST(CliCleanup, ExtendsReferencesAndCleansConditionalTemporaries) {
    CliTestHarness harness("cli_reference_temporaries", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "reference_temporaries");
    harness.workspace.write("src/main.zep", R"zep(
extern fn putchar(value: i32) -> i32
struct Owner {
    public:
        var value: i32
        fn ~Owner() -> void { putchar(value) }
}
fn make(value: i32) -> Owner { return Owner { value: value } }
fn read(value: &Owner) -> i32 { return value.value }
public fn main() -> i32 {
    var reference: &Owner = make(65)
    var value = read(make(66))
    var mut index = 0
    while (index < 2 && read(make(67)) != 0) { index = index + 1 }
    if (read(make(68)) > 0) { return 0 }
    return read(reference) - value
}
)zep");

    for (const auto* level : {"0", "2"}) {
        const auto result = build_and_run(harness, "reference_temporaries", level);

        ASSERT_TRUE(result.succeeded()) << result.stderr_text;
        EXPECT_EQ(result.stdout_text, "BCCDA");
    }
}

TEST(CliCleanup, OrdersDiscardedValuesParametersArraysPayloadsAndGlobals) {
    CliTestHarness harness("cli_cleanup", ZEP_ZEP_EXECUTABLE);
    write_runtime_fixture(harness, TestPaths::project_source_root() / "cli/tests/fixtures/cleanup.test.zep");

    for (const auto* level : {"0", "2"}) {
        const auto result = build_and_run(harness, "cleanup", level);

        ASSERT_TRUE(result.succeeded()) << result.stderr_text;
        EXPECT_EQ(result.stdout_text, "ACDEBBBDCEAGHBF");
    }
}
