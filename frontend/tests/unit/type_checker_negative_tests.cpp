#include <gtest/gtest.h>
#include <string>
#include <vector>

import zep.frontend.test.harness;

TEST(FrontendLifecycle, RejectsInvalidPlacementAndDestruction) {
    EXPECT_TRUE(assert_type_check_error("fn clear(value: i32) -> void { value.~i32() }"));
    EXPECT_TRUE(assert_type_check_error("fn put(ptr: *i32) -> void { new (ptr) 42 }"));
    EXPECT_TRUE(assert_type_check_error("fn empty() -> void {} fn put(ptr: *mut i32) -> void { new (ptr) empty() }"));
    EXPECT_TRUE(assert_type_check_error("fn clear(ptr: *mut void) -> void { ptr->~void() }"));
    EXPECT_TRUE(assert_type_check_error("fn clear(ptr: *mut i32) -> void { ptr->~boolean() }"));
    EXPECT_TRUE(assert_type_check_error("fn clear(ptr: *mut i32) -> void { ptr->~i32(42) }"));
    EXPECT_TRUE(assert_type_check_error("fn clear(value: i32) -> void { delete value }"));
    EXPECT_TRUE(assert_type_check_error("fn clear(value: &i32) -> void { delete value }"));
    EXPECT_TRUE(assert_type_check_error("fn clear(ptr: *mut void) -> void { delete ptr }"));
    EXPECT_TRUE(assert_type_check_error("fn allocate() -> void { new 42 }"));
    EXPECT_TRUE(assert_type_check_error(R"zep(
fn main() -> i32 {
    var mut value = 42
    value.~i32()
    return value
}
)zep"));
}

TEST(FrontendLifecycle, RejectsDeletedCopiesAndDanglingLocalReferences) {
    EXPECT_TRUE(assert_type_check_error(R"zep(
struct Resource {
    public:
        var value: i32
        fn Resource(source: &Resource) -> Resource = delete
}
fn main() -> i32 {
    var source = Resource { value: 1 }
    var copied = source
    return 0
}
)zep"));
    EXPECT_TRUE(assert_type_check_error(R"zep(
fn dangling() -> &i32 {
    var value = 42
    return value
}
)zep"));
    EXPECT_TRUE(assert_type_check_error(R"zep(
struct Record { public: var value: i32 }
fn dangling() -> &i32 {
    var value = Record { value: 42 }
    return value.value
}
)zep"));
    EXPECT_TRUE(assert_type_check_error(R"zep(
fn read(value: &i32) -> i32 { return value }
fn main() -> i32 { return read(null) }
)zep"));
}

TEST(FrontendAssembly, RejectsInvalidOperandContracts) {
    for (const auto* source : {
        R"zep(fn main() -> void { var value = 1 asm { "" out(value, "r", value) } })zep",
        R"zep(fn main() -> void { asm { "{missing}" } })zep",
        R"zep(fn main() -> void { asm { "" options("pure") } })zep",
        R"zep(fn main() -> void { asm { "" options("att", "intel") } })zep"}) {
        EXPECT_TRUE(assert_type_check_error(source));
    }
}

TEST(FrontendTypeCheckerNegative, RejectsReturnTypeMismatch) {
    const std::string source = R"zep(
fn main() -> i32 {
    return true
}
)zep";

    EXPECT_TRUE(assert_type_check_error(source));
}

TEST(FrontendTypeCheckerNegative, RejectsUnknownIdentifier) {
    const std::string source = R"zep(
fn main() -> i32 {
    return missing_value
}
)zep";

    EXPECT_TRUE(assert_type_check_error(source));
}

TEST(FrontendTypeCheckerNegative, RejectsEscapingCapturedClosure) {
    const std::string source = R"zep(
fn main() -> i32 {
    var value = 42
    var closure = { -> value }
    return 0
}
)zep";

    EXPECT_TRUE(assert_type_check_error(source));
}

TEST(FrontendTypeCheckerNegative, RejectsReturningCapturedClosure) {
    const std::string source = R"zep(
fn main() -> () -> i32 {
    var value = 42
    return { -> value }
}
)zep";

    EXPECT_TRUE(assert_type_check_error(source));
}

TEST(FrontendTypeCheckerNegative, RejectsPassingCapturedClosure) {
    const std::string source = R"zep(
fn consume(callback: () -> i32) -> i32 {
    return callback()
}

fn main() -> i32 {
    var value = 42
    return consume({ -> value })
}
)zep";

    EXPECT_TRUE(assert_type_check_error(source));
}

TEST(FrontendTypeCheckerNegative, RejectsInvalidMutablePointerAssignment) {
    const std::string source = R"zep(
fn write(value: *mut i32) -> void {
}

fn main() -> i32 {
    var value: i32 = 1
    write(&value)
    return 0
}
)zep";

    EXPECT_TRUE(assert_type_check_error(source));
}

TEST(FrontendTypeCheckerNegative, RejectsInvalidArraySizeExpression) {
    const std::string source = R"zep(
fn main() -> i32 {
    var size: i32 = 3
    var values: i32[size + 1]
    return #length(values) as i32
}
)zep";

    EXPECT_TRUE(assert_type_check_error(source));
}

TEST(FrontendTypeCheckerNegative, RejectsPrivateStructFieldAccess) {
    const std::string source = R"zep(
struct Secret {
    private:
        var value: i32

    public:
        var visible: i32
}

fn main() -> i32 {
    var secret = Secret { value: 1, visible: 2 }
    return secret.value
}
)zep";

    EXPECT_TRUE(assert_type_check_error(source));
}

TEST(FrontendTypeCheckerNegative, RejectsMissingInterfaceImplementation) {
    const std::string source = R"zep(
interface Shape {
    public:
        fn area() -> i32
}

struct Square : Shape {
    public:
        var size: i32
}

fn main() -> i32 {
    return 0
}
)zep";

    EXPECT_TRUE(assert_type_check_error(source));
}

TEST(FrontendTypeCheckerNegative, RejectsMissingEnumPayloadField) {
    const std::string source = R"zep(
enum Option {
    Some { value: i32 }
}

fn main() -> i32 {
    var item = Option::Some {}
    return 0
}
)zep";

    EXPECT_TRUE(assert_type_check_error(source));
}

TEST(FrontendTypeCheckerNegative, RejectsDuplicateEnumPayloadField) {
    const std::string source = R"zep(
enum Option {
    Some { value: i32 }
}

fn main() -> i32 {
    var item = Option::Some { value: 1, value: 2 }
    return 0
}
)zep";

    EXPECT_TRUE(assert_type_check_error(source));
}

TEST(FrontendTypeCheckerNegative, RejectsInvalidWhenConditionsAndElseOrdering) {
    std::vector<std::string> snippets{
        R"zep(
fn main() -> i32 {
    return when {
        1 -> 1,
        else -> 0,
    }
}
)zep",
        R"zep(
fn main() -> i32 {
    return when {
        else -> 0,
        true -> 1,
    }
}
)zep"};

    for (const auto& snippet : snippets) {
        EXPECT_TRUE(assert_type_check_error(snippet));
    }
}

TEST(FrontendTypeCheckerNegative, RejectsDuplicateNamedArgument) {
    const std::string source = R"zep(
fn add(value: i32) -> i32 {
    return value
}

fn main() -> i32 {
    return add(value: 1, value: 2)
}
)zep";

    EXPECT_TRUE(assert_type_check_error(source));
}

TEST(FrontendTypeCheckerNegative, RejectsUnknownAndMissingNamedArguments) {
    std::vector<std::string> snippets{
        R"zep(
fn add(value: i32) -> i32 {
    return value
}

fn main() -> i32 {
    return add(other: 1)
}
)zep",
        R"zep(
fn add(left: i32, right: i32) -> i32 {
    return left + right
}

fn main() -> i32 {
    return add(1)
}
)zep"};

    for (const auto& snippet : snippets) {
        EXPECT_TRUE(assert_type_check_error(snippet));
    }
}

TEST(FrontendTypeCheckerNegative, RejectsDuplicateFunctionDefinitions) {
    const std::string source = R"zep(
fn main() -> i32 {
    return 0
}

fn main() -> i32 {
    return 1
}
)zep";

    EXPECT_TRUE(assert_type_check_error(source));
}

TEST(FrontendTypeCheckerNegative, RejectsDuplicateSymbolsAndUndeclaredPrimitiveFacade) {
    std::vector<std::string> snippets{
        R"zep(
var value: i32 = 1
var value: i32 = 2

fn main() -> i32 {
    return value
}
)zep",
        R"zep(
fn Integer.next() -> i32 {
    return value + 1
}

fn main() -> i32 {
    return 1.next()
}
)zep"};

    for (const auto& snippet : snippets) {
        EXPECT_TRUE(assert_type_check_error(snippet));
    }
}

TEST(FrontendTypeCheckerNegative, RejectsUseAfterExplicitDestruction) {
    const std::string source = R"zep(
struct Owner {
    public:
        var value: i32
        fn ~Owner() -> void {}
}

fn main() -> i32 {
    var mut owner = Owner { value: 7 }
    owner.~Owner()
    return owner.value
}
)zep";

    FrontendHarness harness(source);
    EXPECT_FALSE(harness.type_check_succeeded());
    ASSERT_EQ(harness.context.diagnostics.entries.size(), 1U);
    EXPECT_EQ(harness.context.diagnostics.entries.front().message,
              "use of destroyed value 'owner'");
}
