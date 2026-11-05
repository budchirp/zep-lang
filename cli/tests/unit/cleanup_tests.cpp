#include <filesystem>
#include <gtest/gtest.h>
#include <string>

import zep.test.support;
import zep.cli.test.support;

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

    for (const auto* level : {"0", "2"}) {
        auto build = harness.run({"build", "-O", level}, harness.workspace.root());
        ASSERT_TRUE(build.succeeded()) << build.stderr_text;
        auto result = TestProcessRunner::run(
            {harness.workspace.file("build/explicit_destruction").string()}, harness.workspace.root());
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

TEST(CliCleanup, ConstructsDeletesAndMovesThroughMemory) {
    CliTestHarness harness("cli_storage_lifetime", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "storage_lifetime");
    harness.workspace.write("src/main.zep", TextFile::read(
        TestPaths::source_root() / "fixtures/storage_lifetime.zep"));

    for (const auto* level : {"0", "1", "2", "3"}) {
        const auto build = harness.run({"build", "-O", level}, harness.workspace.root());
        ASSERT_TRUE(build.succeeded()) << build.stderr_text;

        const auto result = TestProcessRunner::run(
            {harness.workspace.file("build/storage_lifetime").string()}, harness.workspace.root());

        EXPECT_TRUE(result.succeeded()) << result.stderr_text;
    }
}

TEST(CliCleanup, CopiesNestedOwnersAndDestroysRawStorage) {
    CliTestHarness harness("cli_nested_owner_storage", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "nested_owner_storage");
    harness.workspace.write("src/main.zep", R"zep(
import std.text.string.String
import std.collections.vector.Vector
import std.memory.Memory

struct Record {
    public:
        var text: String
}

public fn main() -> i32 {
    var mut original = Record { text: String("original") }
    var copy = original
    original.text = String("replacement")
    if (!copy.text.equals("original")) { return 1 }
    original.text = original.text
    if (!original.text.equals("replacement")) { return 2 }
    var storage = Memory::allocate<String>()
    new (storage) String("stored")
    if (!storage[0].equals("stored")) { return 3 }
    storage[0] = storage[0]
    delete storage
    var mut values = Vector<String>()
    for (var mut index: i32 = 0; index < 12; index = index + 1) { values.push(String("value")) }
    var copied_values = values
    values.clear()
    if (!copied_values.get(11).unwrap()->equals("value")) { return 4 }
    return 0
}
)zep");

    for (const auto* level : {"0", "2"}) {
        auto build = harness.run({"build", "-O", level}, harness.workspace.root());
        ASSERT_TRUE(build.succeeded()) << build.stderr_text;
        auto result = TestProcessRunner::run(
            {harness.workspace.file("build/nested_owner_storage").string()}, harness.workspace.root());
        EXPECT_TRUE(result.succeeded()) << result.stderr_text;
    }
}

TEST(CliCalls, PreservesDistinctGenericCallbackSignatures) {
    CliTestHarness harness("cli_callback_signatures", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "callback_signatures");
    harness.workspace.write("src/main.zep", R"zep(
fn identity<T>(value: T) -> T { return value }
fn increment(value: i32) -> i32 { return value + 1 }
fn twice(value: i32) -> i32 { return value * 2 }
fn invert(value: boolean) -> boolean { return !value }

public fn main() -> i32 {
    var first = identity(increment)
    var second = identity(twice)
    var third = identity(invert)

    if (first(41) != 42 || second(21) != 42 || !third(false)) { return 1 }

    return 0
}
)zep");

    for (const auto* level : {"0", "2"}) {
        auto build = harness.run({"build", "-O", level}, harness.workspace.root());
        ASSERT_TRUE(build.succeeded()) << build.stderr_text;
        auto result = TestProcessRunner::run(
            {harness.workspace.file("build/callback_signatures").string()}, harness.workspace.root());
        EXPECT_TRUE(result.succeeded()) << result.stderr_text;
    }
}

TEST(CliAssembly, ExchangesNamedOperandsAndUsesFixedRegisters) {
    CliTestHarness harness("cli_named_assembly", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "named_assembly");
    harness.workspace.write("src/main.zep", R"zep(
var mut evaluations: i32 = 0

fn source() -> i64 {
    evaluations = evaluations + 1
    return 44
}

fn destination(value: *mut i64) -> *mut i64 {
    evaluations = evaluations + 1
    return value
}

public fn main() -> i32 {
    var mut total: i64 = 20
    asm {
        "addq {rhs}, {sum}"
        in(rhs, "r", 22 as i64)
        inout(sum, "r", total)
        clobber("flags")
    }
    if (total != 42) { return 1 }
    var mut first: i64 = 0
    var mut second: i64 = 0
    asm {
        "movq {source}, {first}";
        "movq {source}, {second}";
        in(source, "r", total)
        out(first, "r", first)
        out(second, "r", second)
        options("pure", "nomem")
    }
    if (first != 42 || second != 42) { return 2 }
    var mut process: i64 = 39
    asm {
        "syscall"
        inout(number, "rax", process)
        clobber("rcx", "r11", "flags")
    }
    if (process <= 0) { return 3 }
    asm {
        "add {sum}, {increment}"
        inout(sum, "r", total)
        in(increment, "i", 1 as i64)
        clobber("flags")
        options("intel", "nomem")
    }
    if (total != 43) { return 4 }
    var mut memory: i64 = 0
    asm {
        "movq {value}, ({address})"
        in(value, "r", total)
        in(address, "r", &mut memory)
    }
    if (memory != 43) { return 5 }

    asm {
        "movq {source}, {target}"
        in(source, "r", source())
        out(target, "r", *destination(&mut memory))
    }

    if (memory != 44 || evaluations != 2) { return 6 }
    return 0
}
)zep");

    for (const auto* level : {"0", "1", "2", "3"}) {
        auto build = harness.run({"build", "-O", level}, harness.workspace.root());
        ASSERT_TRUE(build.succeeded()) << build.stderr_text;
        auto result = TestProcessRunner::run(
            {harness.workspace.file("build/named_assembly").string()}, harness.workspace.root());
        EXPECT_TRUE(result.succeeded()) << result.stderr_text;
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
        auto build = harness.run({"build", "-O", level}, harness.workspace.root());
        ASSERT_TRUE(build.succeeded()) << build.stderr_text;
        auto result = TestProcessRunner::run(
            {harness.workspace.file("build/reference_temporaries").string()}, harness.workspace.root());
        EXPECT_TRUE(result.succeeded()) << result.stderr_text;
        EXPECT_EQ(result.stdout_text, "BCCDA");
    }
}

TEST(CliAssembly, ReportsMalformedInstructions) {
    CliTestHarness harness("cli_invalid_assembly", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "invalid_assembly");
    harness.workspace.write("src/main.zep", R"zep(
public fn main() -> i32 {
    asm { "definitely_invalid_instruction" }
    return 0
}
)zep");
    auto result = harness.run({"build"}, harness.workspace.root());
    EXPECT_FALSE(result.succeeded());
    EXPECT_NE(result.stderr_text.find("instruction"), std::string::npos);
}

TEST(CliCleanup, ExercisesStandardLibraryOwnershipAndErrors) {
    CliTestHarness harness("cli_stdlib_ownership", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "stdlib_ownership");
    harness.workspace.write("src/main.zep", TextFile::read(
        TestPaths::source_root() / "fixtures/stdlib_ownership.zep"));
    for (const auto* level : {"0", "2"}) {
        auto build = harness.run({"build", "-O", level}, harness.workspace.root());
        ASSERT_TRUE(build.succeeded()) << build.stderr_text;
        auto result = TestProcessRunner::run(
            {harness.workspace.file("build/stdlib_ownership").string()}, harness.workspace.root());
        EXPECT_TRUE(result.succeeded()) << result.stderr_text;
    }
}

TEST(CliHttp, HandlesFragmentedInputAndMultipleConnections) {
    CliTestHarness harness("cli_http_loopback", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "http_loopback");
    harness.workspace.write("src/main.zep", TextFile::read(
        TestPaths::source_root() / "fixtures/http_loopback.zep"));
    auto build = harness.run({"build", "-O", "2"}, harness.workspace.root());
    ASSERT_TRUE(build.succeeded()) << build.stderr_text;
    auto result = TestProcessRunner::run(
        {harness.workspace.file("build/http_loopback").string()}, harness.workspace.root());
    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliCleanup, CopiesBasesArraysAndActivePayloads) {
    CliTestHarness harness("cli_lifecycle_aggregates", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "lifecycle_aggregates");
    harness.workspace.write("src/main.zep", TextFile::read(
        TestPaths::source_root() / "fixtures/lifecycle_aggregates.zep"));
    for (const auto* level : {"0", "2"}) {
        auto build = harness.run({"build", "-O", level}, harness.workspace.root());
        ASSERT_TRUE(build.succeeded()) << build.stderr_text;
        auto result = TestProcessRunner::run(
            {harness.workspace.file("build/lifecycle_aggregates").string()}, harness.workspace.root());
        EXPECT_TRUE(result.succeeded()) << result.stderr_text;
    }
}

TEST(CliIo, RetriesInterruptedAndPartialWrites) {
    CliTestHarness harness("cli_partial_writes", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "partial_writes");
    harness.workspace.write("src/main.zep", TextFile::read(
        TestPaths::source_root() / "fixtures/partial_writes.zep"));
    auto build = harness.run({"build", "-O", "2"}, harness.workspace.root());
    ASSERT_TRUE(build.succeeded()) << build.stderr_text;
    auto result = TestProcessRunner::run(
        {harness.workspace.file("build/partial_writes").string()}, harness.workspace.root());
    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliCleanup, OrdersParametersArraysPayloadsAndGlobals) {
    CliTestHarness harness("cli_lifecycle_order", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "lifecycle_order");
    harness.workspace.write("src/main.zep", TextFile::read(
        TestPaths::source_root() / "fixtures/lifecycle_order.zep"));
    for (const auto* level : {"0", "2"}) {
        auto build = harness.run({"build", "-O", level}, harness.workspace.root());
        ASSERT_TRUE(build.succeeded()) << build.stderr_text;
        auto result = TestProcessRunner::run(
            {harness.workspace.file("build/lifecycle_order").string()}, harness.workspace.root());
        EXPECT_TRUE(result.succeeded()) << result.stderr_text;
        EXPECT_EQ(result.stdout_text, "BBDCEAGHBF");
    }
}
