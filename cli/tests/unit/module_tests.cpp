#include <filesystem>
#include <gtest/gtest.h>

import zep.cli.test.support;
import zep.test.support;

TEST(CliZep, CompilesMultiModuleProjectIntoSeparateObjectsAndLinks) {
    CliTestHarness harness("cli_zep_multi_module_objects", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "multi_module_objects");
    harness.workspace.write("src/math.zep", R"zep(public fn add(a: i32, b: i32) -> i32 {
    return a + b
}
)zep");
    harness.workspace.write("src/nested/util.zep", R"zep(public fn multiply(a: i32, b: i32) -> i32 {
    return a * b
}
)zep");
    harness.workspace.write("src/main.zep", R"zep(import math.add
import nested.util.multiply

public fn main() -> i32 {
    if (add(10, 20) != 30) {
        return 1
    }

    if (multiply(6, 7) != 42) {
        return 2
    }

    return 0
}
)zep");

    auto result = build_and_run(harness, "multi_module_objects");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
    EXPECT_TRUE(std::filesystem::exists(harness.workspace.file("build/objs/main.o")));
    EXPECT_TRUE(std::filesystem::exists(harness.workspace.file("build/objs/math.o")));
    EXPECT_TRUE(std::filesystem::exists(harness.workspace.file("build/objs/nested/util.o")));
}

TEST(CliZep, MonomorphizesAcrossMultipleConsumersWithoutDuplicateSymbols) {
    CliTestHarness harness("cli_zep_generic_cross_module", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "generic_cross_module");
    harness.workspace.write("src/generic_container.zep", R"zep(public struct Container<T> {
    public:
        var value: T

        static fn wrap(v: T) -> Container<T> {
            return Container<T> { value: v }
        }

        fn unwrap() -> T {
            return self->value
        }
}

public fn identity<T>(x: T) -> T {
    return x
}
)zep");
    harness.workspace.write("src/consumer_a.zep", R"zep(import generic_container.Container
import generic_container.identity

public fn calculate_a() -> i32 {
    var c = Container<i32>::wrap(10)
    return identity<i32>(c.unwrap()) + 5
}
)zep");
    harness.workspace.write("src/consumer_b.zep", R"zep(import generic_container.Container
import generic_container.identity

public fn calculate_b() -> i32 {
    var c = Container<i32>::wrap(20)
    return identity<i32>(c.unwrap()) + 7
}
)zep");
    harness.workspace.write("src/main.zep", R"zep(import consumer_a.calculate_a
import consumer_b.calculate_b

public fn main() -> i32 {
    if (calculate_a() != 15) {
        return 1
    }

    if (calculate_b() != 27) {
        return 2
    }

    return 0
}
)zep");

    auto result = build_and_run(harness, "generic_cross_module");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
    EXPECT_TRUE(std::filesystem::exists(harness.workspace.file("build/objs/main.o")));
    EXPECT_TRUE(std::filesystem::exists(harness.workspace.file("build/objs/generic_container.o")));
    EXPECT_TRUE(std::filesystem::exists(harness.workspace.file("build/objs/consumer_a.o")));
    EXPECT_TRUE(std::filesystem::exists(harness.workspace.file("build/objs/consumer_b.o")));
}

TEST(CliZep, CompilesNestedModulesWithoutPathCollisions) {
    CliTestHarness harness("cli_zep_nested_path_collision", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "nested_path_collision");
    harness.workspace.write("src/a/b.zep", R"zep(public fn value_ab() -> i32 {
    return 100
}
)zep");
    harness.workspace.write("src/a_b.zep", R"zep(public fn value_a_b() -> i32 {
    return 200
}
)zep");
    harness.workspace.write("src/main.zep", R"zep(import a.b.value_ab
import a_b.value_a_b

public fn main() -> i32 {
    if (value_ab() != 100) {
        return 1
    }

    if (value_a_b() != 200) {
        return 2
    }

    return 0
}
)zep");

    auto result = build_and_run(harness, "nested_path_collision");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
    EXPECT_TRUE(std::filesystem::exists(harness.workspace.file("build/objs/main.o")));
    EXPECT_TRUE(std::filesystem::exists(harness.workspace.file("build/objs/a/b.o")));
    EXPECT_TRUE(std::filesystem::exists(harness.workspace.file("build/objs/a_b.o")));
}
