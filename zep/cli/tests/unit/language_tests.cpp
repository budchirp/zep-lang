#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

import zep.cli.test.support;
import zep.test.support;

TEST(CliLanguage, RunsCheckedErrorsAtBothOptimizationLevels) {
    CliTestHarness harness("cli_checked_errors", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "checked_errors");
    harness.workspace.write("src/errors.test.zep",
        TextFile::read(TestPaths::project_source_root() / "cli/tests/fixtures/errors.test.zep"));

    for (const auto* level : {"0", "2"}) {
        const auto result = harness.run({"test", "-O", level});

        ASSERT_TRUE(result.succeeded()) << result.stderr_text << result.stdout_text;
    }
}

TEST(CliLanguage, KeepsErrorIdentityAcrossPackagesAndModules) {
    CliTestHarness harness("cli_error_identity", ZEP_ZEP_EXECUTABLE);
    harness.workspace.write("zep.json", R"({
        "name":"error_identity",
        "libs":{"std":"0.0.1","dep":{"version":"0.1.0","path":"dep"}}
    })");
    harness.workspace.write("dep/zep.json", R"({
        "name":"dep","version":"0.1.0","type":"library","libs":{"std":"0.0.1"}
    })");
    harness.workspace.write("src/failure.zep",
        "import std.core.Error\npublic struct Failure : Error { public: var code: i32 }\n");
    harness.workspace.write("dep/src/failure.zep",
        "import std.core.Error\npublic struct Failure : Error { public: var code: i64 }\n");
    harness.workspace.write("src/operations.zep", R"zep(
import failure.Failure as LocalFailure
import dep.failure.Failure as ForeignFailure

@throws(LocalFailure, ForeignFailure)
public fn read(remote: boolean) -> i32 {
    if (remote) { throw ForeignFailure { code: 9000000000 } }

    throw LocalFailure { code: 17 }
}
)zep");
    harness.workspace.write("src/main.zep", R"zep(
import failure.Failure as LocalFailure
import dep.failure.Failure as ForeignFailure
import operations.read

public fn main() -> i32 {
    var callback: @throws(ForeignFailure, LocalFailure) (boolean) -> i32 = read
    var first = callback(false) catch { local: LocalFailure -> local.code, remote: ForeignFailure -> 0, }
    var second = callback(true) catch {
        local: LocalFailure -> 0,
        remote: ForeignFailure -> if (remote.code == 9000000000) { 2 } else { 0 },
    }

    return if (first == 17 && second == 2) { 0 } else { 1 }
}
)zep");

    for (const auto* level : {"0", "2"}) {
        const auto result = build_and_run(harness, "error_identity", level);

        ASSERT_TRUE(result.succeeded()) << result.stderr_text;
    }
}

TEST(CliTests, DiscoversRootModulesAndRunsEachTestWithFreshGlobals) {
    CliTestHarness harness("cli_named_discovery", ZEP_ZEP_EXECUTABLE);
    harness.workspace.write("zep.json", R"({
        "name":"named_discovery",
        "libs":{"std":"0.0.1","dep":{"version":"0.1.0","path":"dep"}}
    })");
    harness.workspace.write("dep/zep.json", R"({"name":"dep","version":"0.1.0","type":"library"})");
    harness.workspace.write("dep/src/lib.zep",
        "public fn remote() -> i32 { return 17 }");
    harness.workspace.write("dep/src/lib.test.zep", "test \"dependency hidden\" { missing() }");
    harness.workspace.write("src/main.zep", R"zep(
import nested.nested_value
import shadow.shadow_value
import dep.remote

extern fn putchar(value: i32) -> i32

struct Owner {
    public:
        var value: i32

        fn ~Owner() -> void { putchar(75) }
}

var mut counter = 0
var mut isolated = 3
var mut zero: i32
var mut owner = Owner { value: 1 }
fn hidden() -> i32 { return 42 }
fn current() -> i32 { return counter }
fn owned() -> i32 { return owner.value }
fn identity<T>(value: T) -> T { return value }
public fn main() -> i32 { return hidden() }
@name("main") fn entry() -> i32 { return 43 }
)zep");
    harness.workspace.write("src/main.test.zep", R"zep(
import std.testing.Test
test "same" {
    counter = 7
    owner.value = 9

    Test::expect(current() == 7 && identity(counter) == 7 && owned() == 9)
    Test::expect(isolated == 3 && zero == 0)
}
test "fresh globals" { Test::expect(counter == 0) }
test "application main" {
    Test::expect(main() == 42)
    Test::expect(entry() == 43)
    Test::expect(nested_value() == 9 && shadow_value() == 11 && remote() == 17)
}
)zep");
    harness.workspace.write("src/unimported.test.zep", "test \"same\" {}\n");
    harness.workspace.write("src/__zep_test_runner.zep", "fn helper() -> void {}\n");
    harness.workspace.write("src/nested/index.zep",
        "public fn nested_value() -> i32 { return 9 }\n");
    harness.workspace.write("src/nested/index.test.zep", "test \"nested\" {}\n");
    harness.workspace.write("src/shadow.zep",
        "public fn shadow_value() -> i32 { return 11 }\n");
    harness.workspace.write("src/shadow.test.zep", "test \"file\" {}\n");
    harness.workspace.write("src/shadow/index.test.zep", "test \"index\" {}\n");
    harness.workspace.write("tests/main.test.zep", R"zep(
import std.testing.Test
test "mirrored private access" { Test::expect(hidden() == 42 && owned() == 1) }
)zep");
    harness.workspace.write("tests/fixtures/ignored.test.zep", "test \"fixture\" { missing() }");
    harness.workspace.write("src/tests/main.test.zep", "test \"distinct source path\" {}\n");
    harness.workspace.write("src/src/main/test.zep", "fn distinct_module() -> void {}\n");

    const auto listed = harness.run({"test", "--list"});

    ASSERT_TRUE(listed.succeeded()) << listed.stderr_text;
    EXPECT_EQ(listed.stdout_text.find("dependency hidden"), std::string::npos);
    EXPECT_NE(listed.stdout_text.find("named_discovery.src.unimported.test::same"), std::string::npos);
    EXPECT_NE(listed.stdout_text.find("named_discovery.src.tests.main.test::distinct source path"),
              std::string::npos);
    EXPECT_NE(listed.stdout_text.find("named_discovery.tests.main.test::mirrored private access"),
              std::string::npos);
    EXPECT_LT(listed.stdout_text.find("named_discovery.src.main.test::same"),
              listed.stdout_text.find("named_discovery.src.nested.index.test::nested"));
    EXPECT_FALSE(std::filesystem::exists(harness.workspace.file("build")));

    const auto single = harness.run({"test", "--input",
        harness.workspace.file("tests/main.test.zep").string()});

    ASSERT_TRUE(single.succeeded()) << single.stderr_text << single.stdout_text;
    EXPECT_NE(single.stdout_text.find("1 passed; 0 failed"), std::string::npos);

    const auto filtered = harness.run({"test", "--list", "--filter", "index.test::nested"});

    ASSERT_TRUE(filtered.succeeded()) << filtered.stderr_text;
    EXPECT_EQ(filtered.stdout_text, "named_discovery.src.nested.index.test::nested\n");

    const auto missing = harness.run({"test", "--filter", "does not exist"});

    EXPECT_FALSE(missing.succeeded());
    EXPECT_NE(missing.stderr_text.find("no tests match"), std::string::npos);

    for (const auto* level : {"0", "2"}) {
        const auto result = harness.run({"test", "-O", level});

        ASSERT_TRUE(result.succeeded()) << result.stderr_text << result.stdout_text;
        EXPECT_NE(result.stdout_text.find("9 passed; 0 failed"), std::string::npos);
        EXPECT_EQ(std::count(result.stdout_text.begin(), result.stdout_text.end(), 'K'), 9);
    }
}

TEST(CliTests, ContinuesAfterAssertionsPanicsAndCrashes) {
    CliTestHarness harness("cli_named_failures", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "named_failures");
    harness.workspace.write("src/main.test.zep", R"zep(
import std.testing.Test
import std.io.panic.panic
@name("raise") extern fn signal(number: i32) -> i32

test "custom assertion" { Test::expect(false, "expected mismatch") }
test "default assertion" { Test::expect(false) }
test "panic" { panic("expected panic") }
test "crash" { signal(11) }
test "after failures" { Test::expect(true) }
)zep");

    const auto result = harness.run({"test"});

    EXPECT_FALSE(result.succeeded());
    EXPECT_NE(result.stdout_text.find("PASS named_failures.src.main.test::after failures"), std::string::npos);
    EXPECT_NE(result.stdout_text.find("1 passed; 4 failed"), std::string::npos);
    EXPECT_NE(result.stderr_text.find("expected mismatch"), std::string::npos);
    EXPECT_NE(result.stderr_text.find("test expectation failed"), std::string::npos);
    EXPECT_NE(result.stderr_text.find("expected panic"), std::string::npos);
    EXPECT_NE(result.stderr_text.find("signal 11"), std::string::npos);
}

TEST(CliTests, SeparatesTestFilesFromNormalBuildsAndSupportsSingleFiles) {
    CliTestHarness harness("cli_named_modes", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "named_modes");
    harness.workspace.write("src/main.zep",
        "public fn main() -> i32 { return 0 }");
    harness.workspace.write("src/main.test.zep", "test \"bad\" { missing() }");

    EXPECT_TRUE(harness.run({"build"}).succeeded());
    EXPECT_FALSE(harness.run({"test", "--filter", "other"}).succeeded());

    const auto checked = harness.run({"compile", "--input",
        harness.workspace.file("src/main.test.zep").string()});

    EXPECT_FALSE(checked.succeeded());
    EXPECT_NE(checked.stderr_text.find("missing"), std::string::npos);

    harness.workspace.write("src/main.zep",
        "@name() public fn main() -> i32 { return 0 }");
    harness.workspace.write("src/main.test.zep", "test \"valid\" {}");
    EXPECT_FALSE(harness.run({"test"}).succeeded());

    harness.workspace.write("src/main.zep",
        "public fn main() -> i32 { return 0 } test \"wrong file\" {}");
    const auto misplaced = harness.run({"build"});

    EXPECT_FALSE(misplaced.succeeded());
    EXPECT_NE(misplaced.stderr_text.find(".test.zep"), std::string::npos);

    harness.workspace.write("src/main.zep", "public fn main() -> i32 { return 0 }");

    harness.workspace.write("src/main.test.zep", R"zep(
import std.testing.Test
test "omitted" { Test::fail("must not run in normal builds") }
)zep");
    const auto normal = build_and_run(harness, "named_modes");

    ASSERT_TRUE(normal.succeeded()) << normal.stderr_text;
    EXPECT_TRUE(normal.stdout_text.empty());

    const auto input = harness.workspace.file("sample.test.zep");
    harness.workspace.write("sample.test.zep", "test \"single file\" {}\n");
    const auto single = harness.run({"test", "--input", input.string()});

    ASSERT_TRUE(single.succeeded()) << single.stderr_text;
    EXPECT_NE(single.stdout_text.find("1 passed; 0 failed"), std::string::npos);

    const auto invalid = TestProcessRunner::run(
        {harness.workspace.file("build/tests/runner").string(), "invalid"}, harness.workspace.root());

    EXPECT_EQ(invalid.exit.status, 2);
    EXPECT_FALSE(harness.run({"test", "--project", harness.workspace.root().string(),
        "--input", input.string()}).succeeded());

    harness.workspace.write("empty.test.zep", "fn helper() -> void {}\n");
    const auto empty = harness.run({"test", "--input", harness.workspace.file("empty.test.zep").string()});

    EXPECT_FALSE(empty.succeeded());
    EXPECT_NE(empty.stderr_text.find("no tests found"), std::string::npos);

    harness.workspace.write("zep.json", R"({"name":"named_library","type":"library","libs":{"std":"0.0.1"}})");
    std::filesystem::remove(harness.workspace.file("src/main.zep"));
    harness.workspace.write("src/main.test.zep", "test \"library without entry points\" {}\n");
    const auto library = harness.run({"test"});

    ASSERT_TRUE(library.succeeded()) << library.stderr_text;
    EXPECT_NE(library.stdout_text.find("1 passed; 0 failed"), std::string::npos);
}

TEST(CliCalls, BindsConcreteGenericAndInterfaceCalls) {
    CliTestHarness harness("calls", ZEP_ZEP_EXECUTABLE);
    write_test_group(harness, TestPaths::project_source_root() / "cli/tests/fixtures/calls");
    harness.workspace.write("src/dispatch_types.zep",
        TextFile::read(TestPaths::source_root() / "fixtures/dispatch_types.zep"));

    for (const auto* level : {"0", "1", "2", "3"}) {
        SCOPED_TRACE(level);
        const auto result = harness.run({"test", "-O", level});

        ASSERT_TRUE(result.succeeded()) << result.stderr_text << "exit: " << result.exit.status;
    }
}

TEST(CliLanguage, RunsInferenceAndExhaustiveWhenAtBothOptimizationLevels) {
    CliTestHarness harness("cli_language_improvements", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "language_improvements");
    harness.workspace.write("src/main.zep", R"zep(
struct Box<T> { public: var value: T }
struct Outer<T> { public: var box: Box<T> }
enum Maybe<T> { Some { value: T } None }
fn create() -> Box<i64> { return Box { value: 7 } }
fn consume(box: Box<i64>) -> i64 { return box.value }
fn read<T>(box: Box<T>) -> T { return box.value }
fn choose<T, U>(first: T, second: U) -> T { return first }
fn boolean_code(value: boolean) -> i32 {
    return when (value) { true -> 1, false -> 2, }
}
fn terminating(value: boolean) -> i32 {
    when (value) { true -> { return 3 }, false -> { return 4 }, }
}
public fn main() -> i32 {
    var default_box = Box { value: 1 }
    var box: Box<i64> = Box { value: 2 }
    var outer: Outer<i64> = Outer { box: Box { value: 3 } }
    var maybe: Maybe<i64> = Maybe::Some { value: 4 }
    var result = when (maybe) { Maybe::Some { value } -> value, Maybe::None -> 0 as i64, }
    if (default_box.value != 1 || box.value != 2 || outer.box.value != 3 || result != 4) { return 1 }
    if (create().value != 7 || consume(Box { value: 8 }) != 8) { return 2 }
    if (read(box) != 2 || choose(second: true, first: box.value) != 2) { return 3 }
    if (boolean_code(true) != 1 || boolean_code(false) != 2) { return 4 }
    if (terminating(true) != 3 || terminating(false) != 4) { return 5 }
    return 0
}
)zep");

    for (const auto* level : {"0", "2"}) {
        const auto result = build_and_run(harness, "language_improvements", level);

        ASSERT_TRUE(result.succeeded()) << result.stderr_text << "exit: " << result.exit.status;
    }
}

TEST(CliLanguage, RejectsPatternsFromAnUnrelatedSameNameEnum) {
    CliTestHarness harness("cli_enum_pattern_identity", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "enum_identity");
    harness.workspace.write("src/first.zep", "public enum Choice { First Second }\n");
    harness.workspace.write("src/second.zep", "public enum Choice { First Second }\n");
    harness.workspace.write("src/main.zep", R"zep(
import first.Choice
import second.Choice as OtherChoice
fn read(value: Choice) -> i32 {
    return when (value) { OtherChoice::First -> 1, else -> 0, }
}
public fn main() -> i32 { return read(Choice::First) }
)zep");

    const auto result = harness.run({"build"}, harness.workspace.root());

    EXPECT_FALSE(result.succeeded());
    EXPECT_NE(result.stderr_text.find("when enum pattern type mismatch"), std::string::npos);
}

TEST(CliConst, KeepsDifferentConstSpecializationsDistinct) {
    CliTestHarness harness("cli_const_specializations", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "const_specializations");
    harness.workspace.write("src/main.zep", R"zep(
fn count<const N: i32>() -> i64 {
    var values: i32[N]
    return #length(values)
}
fn forwarded<const N: i32>() -> i64 { return count<N>() }
public fn main() -> i32 {
    if (count<2>() != 2) { return 1 }
    if (count<5>() != 5) { return 2 }
    if (forwarded<7>() != 7) { return 3 }
    if (count<2>() != 2) { return 4 }
    return 0
}
)zep");

    const auto output = build_and_run(harness, "const_specializations");
    EXPECT_TRUE(output.succeeded()) << output.stderr_text;
}

TEST(CliConst, ReportsInvalidSpecializedSizeQueriesWithoutRunningCodegen) {
    CliTestHarness harness("cli_const_unsized", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "const_unsized");
    harness.workspace.write("src/main.zep", R"zep(
fn size<T>() -> i64 { return #sizeof(T) }
public fn main() -> i32 { return size<i32[]>() as i32 }
)zep");

    auto result = harness.run({"build"});

    EXPECT_FALSE(result.succeeded());
    EXPECT_NE(result.stderr_text.find("sizeof requires a concrete sized type"), std::string::npos)
        << result.stderr_text;
}

TEST(CliConst, SpecializesDependentArgumentsWithoutCapturingCallerBindings) {
    CliTestHarness harness("cli_const_dependent_arguments", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "const_dependent_arguments");
    harness.workspace.write("src/main.zep", R"zep(
fn value<const N: i32>() -> i32 { return N }
fn next<const N: i32>() -> i32 { return value<N + 1>() }
public fn main() -> i32 {
    if (next<2>() != 3) { return 1 }
    if (next<8>() != 9) { return 2 }
    if (value<2>() != 2) { return 3 }
    return 0
}
)zep");

    const auto result = build_and_run(harness, "const_dependent_arguments");
    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliConst, EmitsTypedEnumDiscriminants) {
    CliTestHarness harness("cli_enum_discriminants", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "enum_discriminants");
    harness.workspace.write("src/main.zep", R"zep(
enum Signed : i8 { First = -3 Second }
enum Unsigned : u64 { First = 9223372036854775808 Second }
public fn main() -> i32 {
    var signed_value = Signed::Second
    var unsigned_value = Unsigned::Second
    if (signed_value.value != -2) { return 1 }
    if (unsigned_value.value != 9223372036854775809) { return 2 }
    return when (signed_value) { Signed::Second -> 0, else -> 3, }
}
)zep");
    const auto result = build_and_run(harness, "enum_discriminants");
    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}
