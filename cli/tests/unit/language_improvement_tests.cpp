#include <filesystem>
#include <gtest/gtest.h>
#include <string>

import zep.test.support;
import zep.cli.test.support;

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
        auto build = harness.run({"build", "-O", level}, harness.workspace.root());
        ASSERT_TRUE(build.succeeded()) << build.stderr_text;
        auto result = TestProcessRunner::run(
            {harness.workspace.file("build/language_improvements").string()}, harness.workspace.root());
        EXPECT_TRUE(result.succeeded()) << result.stderr_text << "exit: " << result.exit.status;
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

    auto result = harness.run({"build"}, harness.workspace.root());
    EXPECT_FALSE(result.succeeded());
    EXPECT_NE(result.stderr_text.find("when enum pattern type mismatch"), std::string::npos);
}
