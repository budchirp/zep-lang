#include <gtest/gtest.h>

import zep.test.support;

TEST(CliZep, BuildsExecutableFixture) {
    CliTestHarness harness("cli_zep_build", ZEP_ZEP_EXECUTABLE, "fixtures/smoke");
    auto result = harness.run({"build", "-o", "2"}, harness.workspace.file("fixtures/smoke"));

    harness.assert_success_and_exists("fixtures/smoke/build/smoke", result);
}

TEST(CliZep, RejectsInvalidOptimizationLevel) {
    CliTestHarness harness("cli_zep_invalid_optimization", ZEP_ZEP_EXECUTABLE, "fixtures/smoke");
    auto result = harness.run({"build", "-o", "4"}, harness.workspace.file("fixtures/smoke"));

    harness.assert_fails_with_stderr("optimization level must be 0, 1, 2, or 3", result);
}

TEST(CliZep, PrintsAstHirAndIrForFixture) {
    CliTestHarness harness("cli_zep_prints", ZEP_ZEP_EXECUTABLE, "fixtures/smoke");
    auto ir = harness.run({"build", "--verbose"}, harness.workspace.file("fixtures/smoke"));

    EXPECT_TRUE(ir.succeeded()) << ir.stderr_text;
    EXPECT_TRUE(ir.stderr_contains("define i32 @main"));
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
