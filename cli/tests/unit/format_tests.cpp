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

public fn main() -> i32 {
    var mut arguments = FormatArguments()
    arguments.add(7)
    print("stdout {}", arguments).unwrap()

    arguments.clear()
    arguments.add(false)
    eprint("stderr {}", arguments).unwrap()

    return 0
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

public fn main() -> i32 {
    var patterns: cstr[3] = ["{}", "{", "}"]
    var messages: cstr[3] = [
        "missing format argument at index 0",
        "unmatched opening brace at index 0",
        "unmatched closing brace at index 0"
    ]

    for (var mut index = 0; index < 3; index = index + 1) {
        var result = format(patterns[index], FormatArguments())

        if (result.is_ok() || !result.unwrap_error().message().equals(messages[index])) {
            return 1
        }
    }

    var mut arguments = FormatArguments()
    arguments.add(7)
    var extra = format("plain", arguments)

    if (extra.is_ok() || !extra.unwrap_error().message().equals("unused format argument at index 0")) {
        return 2
    }

    return when (format("literal {{ok}}", FormatArguments()).unwrap().equals("literal {ok}")) {
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
