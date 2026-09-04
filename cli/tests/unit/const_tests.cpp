#include <gtest/gtest.h>
#include <string>

import zep.cli.test.support;
import zep.test.support;

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
