#include <gtest/gtest.h>
#include <variant>
#include <string>

import zep.frontend.test.harness;
import zep.frontend.node;
import zep.frontend.sema.symbols;

TEST(FrontendLifecycle, ClassifiesReferenceCopiesWithoutRawPointerCompatibility) {
    FrontendHarness harness(R"zep(
struct Raw {
    public:
        var value: i32

        fn Raw(source: *Raw) -> Raw {
            return Raw { value: (*source).value }
        }
}

struct Owner {
    public:
        var value: i32

        fn Owner(source: &Owner) -> Owner = default
}
)zep");

    ASSERT_TRUE(harness.type_check_succeeded());

    const auto* raw = harness.program.statements[0]->as<StructDeclaration>();
    const auto* owner = harness.program.statements[1]->as<StructDeclaration>();

    ASSERT_NE(raw, nullptr);
    ASSERT_NE(owner, nullptr);
    ASSERT_EQ(raw->methods.size(), 1U);
    ASSERT_EQ(owner->methods.size(), 1U);

    EXPECT_EQ(raw->methods.front()->function_symbol->role,
              FunctionSymbol::Role(ConstructorKind::Type::Value));
    EXPECT_EQ(owner->methods.front()->function_symbol->role,
              FunctionSymbol::Role(ConstructorKind::Type::Copy));
}

TEST(FrontendLifecycle, RechecksNestedGenericReferenceCallsWithoutChangingTheirTypes) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
fn forward<T>(value: &T) -> move T { return &value as move T }
fn put<T>(ptr: *mut T, value: move T) -> void { new (ptr) value }
fn build<T>(ptr: *mut T, value: T) -> void { put(ptr, forward(value)) }
struct Resource {
    public:
        var value: i32
        fn Resource(source: &Resource) -> Resource = delete
        fn Resource(source: move Resource) -> Resource = default
}
fn main() -> i32 {
    var value = Resource { value: 42 }
    var moved = forward(value)
    return moved.value
}
)zep"));
}

TEST(FrontendLifecycle, ConstructsAndDestroysValuesWithoutBuiltinCalls) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
struct Record { public: var value: i32 }
fn initialize(ptr: *mut Record, value: &Record) -> void {
    new (ptr) value
}
fn clear(ptr: *mut Record) -> void {
    ptr->~Record()
}
fn main() -> i32 {
    var mut value = Record { value: 42 }
    value.~Record()
    value = Record { value: 7 }
    return value.value
}
)zep"));
}

TEST(FrontendLifecycle, ResolvesHeapOperationsThroughMemory) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
struct Memory {
    public:
        static fn allocate_bytes(size: i64) -> *mut void { return null }
        static fn free(ptr: *mut void) -> void {}
}
struct Record { public: var value: i32 }
fn check() -> void {
    var record = new Record { value: 42 }
    delete record
    var number = new 42
    delete number
    var missing: *mut Record = null
    delete missing
    delete null
}
)zep"));
}

TEST(FrontendLifecycle, AcceptsReferencesMovesAndDefaultedSpecialMembers) {
    auto source = R"zep(
struct Record {
    public:
        var value: i32
        fn Record(source: &Record) -> Record = default
        fn Record(source: move Record) -> Record = default
        fn operator=(source: &Record) mut -> &mut Record = default
        fn operator=(source: move Record) mut -> &mut Record = default
}
fn read(value: &Record) -> i32 { return value.value }
fn forward<T>(value: &T) -> move T { return &value as move T }
fn main() -> i32 {
    var mut first = Record { value: 42 }
    var copied = first
    var moved = forward(first)
    var reference: &Record = copied
    first = moved
    return read(reference)
}
)zep";
    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendAssembly, AcceptsNamedOperands) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
fn main() -> i32 {
    var mut value: i64 = 1
    asm {
        "addq {input}, {output}"
        in(input, "r", 2 as i64)
        inout(output, "r", value)
        clobber("flags")
    }
    return value as i32
}
)zep"));
}

TEST(FrontendTypeCheckerPositive, KeepsGenericLookaheadInsideExpressionDelimiters) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
fn valid(count: i64, width: i64) -> boolean {
    return (count < 0 || width < 0 || (width > 0 && count > 100 / width))
}
)zep"));
}

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

TEST(FrontendTypeCheckerPositive, AcceptsInheritedMutableMethodCalls) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
struct Handle {
    public:
        var value: i32
        fn close() mut -> void { value = -1 }
}

struct Stream : Handle {}

fn main() -> i32 {
    var mut stream = Stream { value: 42 }
    stream.close()
    return stream.value
}
)zep"));
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

TEST(FrontendTypeCheckerPositive, UsesDeclaredCopyConstructorForAggregateLvalues) {
    const std::string source = R"zep(
struct Owner {
    public:
        var value: i32

        fn Owner(source: &Owner) -> Owner {
            return Owner { value: source.value }
        }

        fn ~Owner() -> void {}
}

fn consume(value: Owner) -> i32 {
    return value.value
}

fn main() -> i32 {
    var first = Owner { value: 7 }
    var second = first
    return consume(second)
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}
