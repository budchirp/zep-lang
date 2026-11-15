#include <gtest/gtest.h>
#include <string>

import zep.test.support;
import zep.cli.test.support;

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
        SCOPED_TRACE(level);
        const auto result = build_and_run(harness, "named_assembly", level);

        ASSERT_TRUE(result.succeeded()) << result.stderr_text;
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

    const auto result = harness.run({"build"}, harness.workspace.root());

    EXPECT_FALSE(result.succeeded());
    EXPECT_NE(result.stderr_text.find("instruction"), std::string::npos);
}
