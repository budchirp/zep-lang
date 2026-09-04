#include <gtest/gtest.h>
#include <string>
#include <vector>

import zep.frontend.test.harness;

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
        EXPECT_TRUE(FrontendChecks::type_check_error(snippet)) << snippet;
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
        EXPECT_TRUE(FrontendChecks::type_check_error(snippet)) << snippet;
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
