#include <filesystem>
#include <gtest/gtest.h>
#include <string>

import zep.test.support;
import zep.cli.test.support;

TEST(CliCleanup, ExercisesStandardLibraryOwnershipAndErrors) {
    CliTestHarness harness("cli_stdlib", ZEP_ZEP_EXECUTABLE);
    write_test_group(harness, "std/tests");

    for (const auto* level : {"0", "2"}) {
        const auto result = harness.run({"test", "-O", level});

        ASSERT_TRUE(result.succeeded()) << result.stderr_text << "exit: " << result.exit.status;
    }
}

TEST(CliCleanup, OwnsFfiHandlesAndUsesConsistentResourceApis) {
    for (const auto* fixture : {"ffi_ownership", "format_failures"}) {
        SCOPED_TRACE(fixture);
        CliTestHarness harness(fixture, ZEP_ZEP_EXECUTABLE);
        write_runtime_fixture(harness, std::filesystem::path("std/tests/fixtures") /
                              (std::string(fixture) + ".test.zep"));

        const auto result = build_and_run(harness, fixture, "2");

        ASSERT_TRUE(result.succeeded()) << result.stderr_text;
    }
}

TEST(CliHttp, HandlesFragmentedInputAndMultipleConnections) {
    CliTestHarness harness("cli_http_loopback", ZEP_ZEP_EXECUTABLE);
    write_runtime_fixture(harness, "std/tests/fixtures/http_loopback.test.zep");

    const auto result = build_and_run(harness, "http_loopback", "2");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliIo, HandlesBinaryTextAndInterruptedPartialWrites) {
    CliTestHarness harness("cli_text_io", ZEP_ZEP_EXECUTABLE);
    write_runtime_fixture(harness, "std/tests/fixtures/text_io.test.zep");

    const auto result = build_and_run(harness, "text_io", "2");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text << "exit: " << result.exit.status;
}
