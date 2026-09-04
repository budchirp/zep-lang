#include <gtest/gtest.h>
#include <string>

import zep.frontend.test.harness;

TEST(FrontendTypeCheckerPositive, AcceptsNumericLiteralsCastsAndReturnCompatibility) {
    const std::string source = R"zep(
fn main() -> i64 {
    var small: i32 = 7
    var wide: i64 = small as i64
    return wide + 2 as i64
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendTypeCheckerPositive, AcceptsPointerMutabilityAndVoidPointerCompatibility) {
    const std::string source = R"zep(
fn read(value: *i32) -> i32 {
    return *value
}

fn main() -> i32 {
    var mut value: i32 = 7
    var pointer: *mut i32 = &mut value
    var erased: *void = pointer
    return read(pointer)
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendTypeCheckerPositive, AcceptsFixedArraysAndLengthBuiltin) {
    const std::string source = R"zep(
fn main() -> i32 {
    var values: i32[3]
    return #length(values) as i32
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendTypeCheckerPositive, AcceptsScopedBlockExpressionResult) {
    const std::string source = R"zep(
fn main() -> i32 {
    return do {
        var value = 40
        value + 2
    }
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendTypeCheckerPositive, AcceptsDirectCapturedClosureInvocation) {
    const std::string source = R"zep(
fn main() -> i32 {
    var value = 42
    return { -> value }()
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendTypeCheckerPositive, AcceptsStructDefaultsAndMethods) {
    const std::string source = R"zep(
struct Box {
    public:
        var value: i32 = 7

        fn read() -> i32 {
            return value
        }
}

fn main() -> i32 {
    var box = Box { value: 7 }
    return box.read()
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendTypeCheckerPositive, AcceptsStructInheritanceAndBaseCompatibility) {
    const std::string source = R"zep(
struct Base {
    public:
        var value: i32
}

struct Child : Base {
    public:
        var extra: i32
}

fn take(value: Base) -> i32 {
    return value.value
}

fn main() -> i32 {
    var child = Child { value: 1, extra: 2 }
    return take(child)
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendTypeCheckerPositive, AcceptsInterfaceImplementation) {
    const std::string source = R"zep(
interface Shape {
    public:
        fn area() -> i32
}

struct Square : Shape {
    public:
        var size: i32

        override fn area() -> i32 {
            return size * size
        }
}

fn main() -> i32 {
    var square = Square { size: 4 }
    return square.area()
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendTypeCheckerPositive, AcceptsInterfaceInheritance) {
    const std::string source = R"zep(
interface Reader {
    public:
        fn read() -> i32
}

interface Cursor : Reader {
    public:
        fn seek() -> i32
}

struct File : Cursor {
    public:
        override fn read() -> i32 {
            return 1
        }

        override fn seek() -> i32 {
            return 2
        }
}

fn main() -> i32 {
    var file = File {}
    return file.read() + file.seek()
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendTypeCheckerPositive, AcceptsOverloadResolutionNamedArgumentsAndExtensionMethods) {
    const std::string source = R"zep(
struct Count {
    public:
        var value: i32
}

fn add(a: i32, b: i32) -> i32 {
    return a + b
}

fn choose(value: i32) -> i32 {
    return value
}

fn choose(value: i64) -> i64 {
    return value
}

fn Count.next() -> i32 {
    return value + 1
}

fn main() -> i32 {
    var count = Count { value: 3 }
    return add(b: 2, a: 1) + choose(4) + count.next()
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendTypeCheckerPositive, AcceptsFunctionValuedIdentifierCalls) {
    const std::string source = R"zep(
fn increment(value: i32) -> i32 {
    return value + 1
}

fn apply(handler: (i32) -> i32) -> i32 {
    return handler(1)
}

fn main() -> i32 {
    var handler: (i32) -> i32 = increment
    return apply(handler)
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendTypeCheckerPositive, AcceptsStaticExtensionsThroughDotAndQualifiedAccess) {
    const std::string source = R"zep(
struct Count {
    public:
        var value: i32
}

static fn Count.zero() -> Count {
    return Count { value: 0 }
}

fn main() -> i32 {
    return Count.zero().value + Count::zero().value
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendTypeCheckerPositive, AcceptsCloneWrapperVectorStringResolution) {
    const std::string source = R"zep(
interface Clone<T> {
    public:
        fn clone() -> T
}

struct AllocatorString : Clone<AllocatorString> {
    public:
        var value: i32

        override fn clone() -> AllocatorString {
            return AllocatorString { value: value }
        }
}

struct String : Clone<String> {
    public:
        var inner: AllocatorString

        override fn clone() -> String {
            return String { inner: inner.clone() }
        }
}

struct Vector<T> : Clone<Vector<T>> {
    public:
        var value: T

        override fn clone() -> Vector<T> {
            return Vector<T> { value: value }
        }
}

fn main() -> i32 {
    var text = String { inner: AllocatorString { value: 7 } }
    var values = Vector<String> { value: text.clone() }
    var copied = values.clone()
    return copied.value.inner.value
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendTypeCheckerPositive, AcceptsFunctionTypeAliasesWithNamedAggregateTypes) {
    const std::string source = R"zep(
struct Request {
    public:
        var path: cstr
}

struct Response {
    public:
        var status: i32
}

type Handler = (*Request) -> Response

fn health(request: *Request) -> Response {
    return Response { status: 200 }
}

fn call(handler: Handler, request: *Request) -> Response {
    return handler(request)
}

fn main() -> i32 {
    var request = Request { path: "/health" }
    var handler: Handler = health
    var response = call(handler, &request)
    return response.status
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}
