#include <filesystem>
#include <gtest/gtest.h>
#include <string>

import zep.test.support;
import zep.cli.test.support;

TEST(CliCleanup, ExercisesStandardLibraryOwnershipAndErrors) {
    CliTestHarness harness("cli_stdlib", ZEP_ZEP_EXECUTABLE);
    write_test_group(harness, TestPaths::standard_library() / "tests");

    for (const auto* level : {"0", "2"}) {
        const auto result = harness.run({"test", "-O", level});

        ASSERT_TRUE(result.succeeded()) << result.stderr_text << "exit: " << result.exit.status;
    }
}

TEST(CliCleanup, OwnsFfiHandlesAndUsesConsistentResourceApis) {
    for (const auto* fixture : {"ffi_ownership", "format_failures"}) {
        SCOPED_TRACE(fixture);
        CliTestHarness harness(fixture, ZEP_ZEP_EXECUTABLE);
        write_runtime_fixture(harness, TestPaths::standard_library() / "tests/fixtures" /
                              (std::string(fixture) + ".test.zep"));

        const auto result = build_and_run(harness, fixture, "2");

        ASSERT_TRUE(result.succeeded()) << result.stderr_text;
    }
}

TEST(CliHttp, HandlesFragmentedInputAndMultipleConnections) {
    CliTestHarness harness("cli_http_loopback", ZEP_ZEP_EXECUTABLE);
    write_runtime_fixture(harness, TestPaths::standard_library() / "tests/fixtures/http_loopback.test.zep");

    const auto result = build_and_run(harness, "http_loopback", "2");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliHttp, ChecksNotesStorageApiAndClientCommands) {
    for (const auto* project : {"server", "client"}) {
        SCOPED_TRACE(project);
        CliTestHarness harness(std::string("notes_") + project, ZEP_ZEP_EXECUTABLE);
        write_project_config(harness, std::string("notes_") + project);
        const auto root = TestPaths::repository_root() / "examples/notes" / project;

        for (const auto* directory : {"src", "tests"}) {
            for (const auto& entry : std::filesystem::directory_iterator(root / directory)) {
                if (entry.is_regular_file()) {
                    harness.workspace.write(std::filesystem::path(directory) / entry.path().filename(),
                                            TextFile::read(entry.path()));
                }
            }
        }

        const auto result = harness.run({"test", "-O", "2"});

        ASSERT_TRUE(result.succeeded()) << result.stderr_text;
    }
}

TEST(CliIo, HandlesBinaryTextAndInterruptedPartialWrites) {
    CliTestHarness harness("cli_text_io", ZEP_ZEP_EXECUTABLE);
    write_runtime_fixture(harness, TestPaths::standard_library() / "tests/fixtures/text_io.test.zep");

    const auto result = build_and_run(harness, "text_io", "2");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text << "exit: " << result.exit.status;
}
