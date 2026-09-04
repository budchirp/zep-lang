#include <gtest/gtest.h>
#include <string>

import zep.frontend.test.harness;
import zep.frontend.node;
import zep.frontend.sema.kind;

TEST(FrontendParserMatrix, ParsesDirectImportsAndAliases) {
    const std::string source = R"zep(
import math.ops.add
import math.ops.subtract as minus
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesAttributesOnFunctionsAndTypes) {
    const std::string source = R"zep(
@name("renamed_answer")
fn answer() -> i32 {
    return 42
}

@target("linux")
struct LinuxOnly {
    public:
        var value: i32 = 1
}

@mangle(false)
extern fn puts(value: cstr) -> i32
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesGenericTypeAliases) {
    const std::string source = R"zep(
struct Pair<A, B> {
    public:
        var first: A
        var second: B
}

type IntPair = Pair<i32, i32>
type Alias<T> = Pair<T, T>
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesExternFunctionsVarsAndVariadicPrototypes) {
    const std::string source = R"zep(
extern fn printf(format: cstr, ...) -> i32
extern fn memcpy(destination: *mut void, source: *void, count: i64) -> *mut void
extern var errno: i32
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesStructVisibilityDefaultsMethodsNestedTypesAndInheritance) {
    const std::string source = R"zep(
struct Node {
    public:
        var id: i32
}

struct Entity : Node {
    public:
        var value: i32 = 0

        static fn make(value: i32) -> Entity {
            return Entity { id: 0, value: value }
        }

        fn set(value: i32) mut -> void {
            self.value = value
        }

    private:
        var hidden: i32 = 1

        struct Meta {
            public:
                var label: cstr
        }

        enum Kind {
            Basic
            Advanced
        }
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesEnumsWithPayloadsBackingGenericsAndMethods) {
    const std::string source = R"zep(
enum Status : i32 {
    Ok = 0
    Failed = 1

    public:
        fn is_ok() -> boolean {
            return value == 0
        }
}

enum Option<T> {
    None
    Some { value: T }

    public:
        fn is_some() -> boolean {
            return true
        }
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesInterfaceInheritanceAndMutableMethods) {
    const std::string source = R"zep(
interface Readable {
    public:
        fn read() -> i32
}

interface Writer : Readable {
    public:
        fn write(value: i32) mut -> void
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesConstGenericFunctionsAndNamedGenericCalls) {
    const std::string source = R"zep(
fn identity<T>(value: T) -> T {
    return value
}

fn pick<const N: i32, T>(value: T) -> T {
    return value
}

fn main() -> i32 {
    return pick<N: 3, T: i32>(identity<i32>(1))
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesComplexTypeExpressions) {
    const std::string source = R"zep(
struct Box<T> {
    public:
        var value: T
}

fn accept(
    pointer: *mut Box<i32>,
    matrix: i32[2][3],
    callback: (i32, *void) -> boolean,
    qualified: math.types.Pair<i32, cstr>
) -> void {
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesMemberIndexPointerAndStaticAccess) {
    const std::string source = R"zep(
fn main() -> i32 {
    var mut values: i32[3]
    var pointer: *mut i32 = &mut values[0]
    pointer->value = Math::identity(values[1])
    return pointer->value
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesIfWhenSubjectAndSubjectlessExpressions) {
    const std::string source = R"zep(
enum Flag {
    On
    Off
}

fn main(flag: Flag, condition: boolean) -> i32 {
    var value = if (condition) {
        1
    } else {
        2
    }

    return when (flag) {
        Flag::On -> value,
        Flag::Off if (condition) -> 3,
        else -> when {
            condition -> 4,
            else -> 0,
        },
    }
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesWhileCStyleForRangeForAndDefer) {
    const std::string source = R"zep(
fn main() -> i32 {
    var mut total: i32 = 0
    while (total < 3) {
        total = total + 1
    }

    for (var mut i: i32 = 0; i < 4; i = i + 1) {
        total = total + i
    }

    defer {
        total = total + 1
    }

    return total
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesAssignmentUnaryCastIsAndPrecedence) {
    const std::string source = R"zep(
fn main(value: any) -> i32 {
    var mut result: i32 = 0
    result = -1 + +2 * 3 as i32

    if (!(value is i32) || result <= 0) {
        return 0
    }

    return result
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesStructAndEnumLiteralsWithNamedPayloads) {
    const std::string source = R"zep(
struct Point {
    public:
        var x: i32
        var y: i32
}

enum MaybePoint {
    None
    Some { value: Point }
}

fn main() -> MaybePoint {
    return MaybePoint::Some { value: Point { x: 1, y: 2 } }
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesTypedAndInferredClosures) {
    const std::string source = R"zep(
fn main() -> void {
    var typed = { left: i32, right: i32 -> left + right }
    var inferred = { value -> value }
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ParsesScopedBlockExpressions) {
    const std::string source = R"zep(
fn main() -> i32 {
    return do {
        var value = 40
        value + 2
    }
}
)zep";

    EXPECT_TRUE(assert_parse_ok(source));
}

TEST(FrontendParserMatrix, ReportsParserErrorsWithoutExiting) {
    FrontendHarness legacy_import("import math { add }\n");
    EXPECT_FALSE(legacy_import.parse_succeeded());
    ASSERT_FALSE(legacy_import.context.diagnostics.entries.empty());

    FrontendHarness public_import("public import math.add\n");
    EXPECT_FALSE(public_import.parse_succeeded());
    ASSERT_FALSE(public_import.context.diagnostics.entries.empty());

    FrontendHarness unknown_builtin("fn main() -> i32 { return #missing(i32) }\n");
    EXPECT_FALSE(unknown_builtin.parse_succeeded());
    ASSERT_FALSE(unknown_builtin.context.diagnostics.entries.empty());
    EXPECT_EQ(unknown_builtin.context.diagnostics.entries.front().message,
              "unknown builtin function '#missing'");
}
