#include <filesystem>
#include <gtest/gtest.h>

import zep.test.support;

TEST(CliZep, BuildsExecutableFixture) {
    CliTestHarness harness("cli_zep_build", ZEP_ZEP_EXECUTABLE, "fixtures/smoke");
    auto result = harness.run({"build", "-O", "2"}, harness.workspace.file("fixtures/smoke"));

    harness.assert_success_and_exists("fixtures/smoke/build/smoke", result);
}

TEST(CliZep, RejectsInvalidOptimizationLevel) {
    CliTestHarness harness("cli_zep_invalid_optimization", ZEP_ZEP_EXECUTABLE, "fixtures/smoke");
    auto result = harness.run({"build", "-O", "4"}, harness.workspace.file("fixtures/smoke"));

    harness.assert_fails_with_stderr("optimization level must be 0, 1, 2, or 3", result);
}

TEST(CliZep, PrintsAstHirAndIrForFixture) {
    CliTestHarness harness("cli_zep_prints", ZEP_ZEP_EXECUTABLE, "fixtures/smoke");
    auto ir = harness.run({"build", "--emit-ir"}, harness.workspace.file("fixtures/smoke"));

    EXPECT_TRUE(ir.succeeded()) << ir.stderr_text;
    EXPECT_NE(ir.stderr_text.find("define i32 @main"), std::string::npos);
}

TEST(CliZep, FailsForMissingProjectConfig) {
    CliTestHarness harness("cli_zep_missing_config", ZEP_ZEP_EXECUTABLE);
    auto result = harness.run({"build"}, harness.workspace.root());

    harness.assert_fails_with_stderr("could not find zep.json", result);
}

TEST(CliZep, FetchFailsForMissingProjectConfig) {
    CliTestHarness harness("cli_zep_fetch_missing_config", ZEP_ZEP_EXECUTABLE);
    auto result = harness.run({"fetch"}, harness.workspace.root());

    harness.assert_fails_with_stderr("could not find zep.json", result);
}

TEST(CliZep, CompilesSingleFileWithIndependentOutputAndOptimizationOptions) {
    CliTestHarness harness("cli_zep_compile", ZEP_ZEP_EXECUTABLE);
    harness.workspace.write("source.zep", "fn main() -> i32 { return 0 }");
    auto output = harness.workspace.file("custom.o");

    auto result = harness.run({"compile", "--input", harness.workspace.file("source.zep").string(),
                               "-o", output.string(), "-O", "1", "--emit-ir"},
                              harness.workspace.root());

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
    EXPECT_TRUE(std::filesystem::is_regular_file(output));
}

TEST(CliZep, RejectsUnsupportedCompileTarget) {
    CliTestHarness harness("cli_zep_compile_target", ZEP_ZEP_EXECUTABLE);
    harness.workspace.write("source.zep", "fn main() -> i32 { return 0 }");

    auto result = harness.run({"compile", "--input", harness.workspace.file("source.zep").string(),
                               "--target", "not-a-real-target"},
                              harness.workspace.root());

    harness.assert_fails_with_stderr("unsupported target 'not-a-real-target'", result);
}
