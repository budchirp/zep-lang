#include <gtest/gtest.h>

import zep.test.support;
import zep.cli.test.support;

TEST(CliZep, RunsStdlibFormattingFixture) {
    CliTestHarness harness("cli_zep_stdlib_format", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "stdlib_format");
    harness.workspace.write("src/main.zep", R"zep(
import std.io.FormatArguments
import std.io.format
import std.io.eprint
import std.io.print
import std.io.FormatError
import std.io.IoError

public fn main() -> i32 {
    return try {
        var mut arguments = FormatArguments()
        arguments.add(7)
        print("stdout {}", arguments)

        arguments.clear()
        arguments.add(false)
        eprint("stderr {}", arguments)

        0
    } catch {
        error: FormatError -> 1,
        error: IoError -> 2,
    }
}
)zep");

    const auto result = build_and_run(harness, "stdlib_format");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
    EXPECT_EQ(result.stdout_text, "stdout 7");
    EXPECT_EQ(result.stderr_text, "stderr false");
}

TEST(CliFormat, ReportsSyntaxErrorsAndPreservesEscapedBraces) {
    CliTestHarness harness("cli_format_syntax", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "format_syntax");
    harness.workspace.write("src/main.zep", R"zep(
import std.io.FormatArguments
import std.io.format
import std.io.FormatError

public fn main() -> i32 {
    var patterns: cstr[3] = ["{}", "{", "}"]
    var messages: cstr[3] = [
        "missing format argument at index 0",
        "unmatched opening brace at index 0",
        "unmatched closing brace at index 0"
    ]

    for (var mut index = 0; index < 3; index = index + 1) {
        try {
            format(patterns[index], FormatArguments())
            return 1
        } catch { error: FormatError -> {
            if (!error.message().equals(messages[index])) {
                return 1
            }
        }, }
    }

    var mut arguments = FormatArguments()
    arguments.add(7)
    try {
        format("plain", arguments)
        return 2
    } catch { error: FormatError -> {
        if (!error.message().equals("unused format argument at index 0")) {
            return 2
        }
    }, }

    var escaped = format("literal {{ok}}", FormatArguments()) catch { error: FormatError -> { return 3 }, }

    return when (escaped.equals("literal {ok}")) {
        true -> 0,
        false -> 3,
    }
}
)zep");

    const auto result = build_and_run(harness, "format_syntax");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliZep, RunsStdlibPanicFixture) {
    CliTestHarness harness("cli_zep_stdlib_panic", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "stdlib_panic");
    harness.workspace.write("src/main.zep", R"zep(
import std.io.panic.panic

public fn main() -> i32 {
    panic("fatal error")
}
)zep");

    const auto result = build_and_run(harness, "stdlib_panic");

    EXPECT_FALSE(result.succeeded());
    EXPECT_EQ(result.exit.status, 1);
    EXPECT_TRUE(result.stdout_text.empty());
    EXPECT_EQ(result.stderr_text, "fatal error\n");
}
