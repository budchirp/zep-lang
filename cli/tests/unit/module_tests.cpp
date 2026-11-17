#include <filesystem>
#include <gtest/gtest.h>

import zep.cli.test.support;
import zep.test.support;

TEST(CliZep, LinksPrivateHelpersUsedByImportedGenericFunctions) {
    CliTestHarness harness("cli_generic_private_helper", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "generic_private_helper");
    harness.workspace.write("src/library.zep", R"zep(
var mut calls = 0
fn helper() -> i32 { calls = calls + 1 return calls }
public fn compute<T>(value: T) -> i32 { return helper() + (#sizeof(T) as i32) }
public fn count() -> i32 { return calls }
)zep");
    harness.workspace.write("src/first.zep", R"zep(
import library.compute
public fn first() -> i32 { return compute(1) }
)zep");
    harness.workspace.write("src/second.zep", R"zep(
import library.compute
public fn second() -> i32 { return compute(2) }
)zep");
    harness.workspace.write("src/main.zep", R"zep(
import first.first
import second.second
import library.count
public fn main() -> i32 {
    if (first() != 5 || second() != 6 || count() != 2) { return 1 }
    return 0
}
)zep");

    const auto result = build_and_run(harness, "generic_private_helper", "2");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliZep, DistinguishesSameNamedNominalAndGenericDeclarationsAcrossModules) {
    CliTestHarness harness("cli_generic_scoped_types", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "generic_scoped_types");
    harness.workspace.write("src/common.zep", R"zep(
public fn measure<T>(value: T) -> i64 { return #sizeof(T) }
)zep");
    harness.workspace.write("src/first.zep", R"zep(
import common.measure
struct Tag { public: var value: i32 }
struct Cell<T> { public: var value: T }
public fn read() -> i64 {
    return measure(Tag { value: 1 }) + measure(Cell<i32> { value: 2 })
}
)zep");
    harness.workspace.write("src/second.zep", R"zep(
import common.measure
struct Tag { public: var first: i64 var second: i64 }
struct Cell<T> { public: var value: T var other: i64 }
public fn read() -> i64 {
    return measure(Tag { first: 1, second: 2 }) + measure(Cell<i32> { value: 3, other: 4 })
}
)zep");
    harness.workspace.write("src/main.zep", R"zep(
import first.read as read_first
import second.read as read_second
public fn main() -> i32 {
    if (read_first() != 8 as i64 || read_second() != 32 as i64) { return 1 }
    return 0
}
)zep");

    const auto result = build_and_run(harness, "generic_scoped_types", "2");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}

TEST(CliZep, RejectsDuplicateExplicitLinkerNames) {
    CliTestHarness harness("cli_duplicate_linker_names", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "duplicate_linker_names");
    harness.workspace.write("src/first.zep",
        "@name(\"collision\") public fn first() -> i32 { return 1 }\n");
    harness.workspace.write("src/second.zep",
        "@name(\"collision\") public fn second() -> i32 { return 2 }\n");
    harness.workspace.write("src/main.zep", R"zep(
import first.first
import second.second
public fn main() -> i32 { return first() + second() }
)zep");

    const auto result = harness.run({"build"});

    EXPECT_FALSE(result.succeeded());
    EXPECT_NE(result.stderr_text.find("duplicate symbol: collision"), std::string::npos);
}

TEST(CliZep, PreservesExplicitLinkerNamesEqualToSourceNames) {
    CliTestHarness harness("cli_explicit_linker_name", ZEP_ZEP_EXECUTABLE);
    write_project_config(harness, "explicit_linker_name");
    harness.workspace.write("src/owner.zep",
        "@name(\"same\") public fn same(value: i32) -> i32 { return value + 1 }\n");
    harness.workspace.write("src/main.zep", R"zep(
import owner.same
@name("same") extern fn native(value: i32) -> i32
public fn main() -> i32 {
    if (same(1) != 2 || native(2) != 3) { return 1 }
    return 0
}
)zep");

    const auto result = build_and_run(harness, "explicit_linker_name", "2");

    EXPECT_TRUE(result.succeeded()) << result.stderr_text;
}
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
