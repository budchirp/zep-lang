#include <gtest/gtest.h>
#include <string>
#include <vector>

import zep.frontend.test.harness;
import zep.frontend.node;
import zep.frontend.sema.types;
import zep.frontend.sema.types.resolver;
import zep.frontend.sema.expression.generic;
import zep.common.source.span;

TEST(FrontendInference, ContextualizesInstantiatedReceiverArguments) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
enum Outcome<T, E> { Ok { value: T } Error { error: E } }
struct Sink<T> {
    public:
        fn put(value: T) mut -> void {}
}
fn check() -> void {
    var mut sink: Sink<Outcome<i64, i32>> = Sink {}
    sink.put(Outcome::Ok { value: 42 })
}
)zep"));
}

TEST(FrontendInference, ResolvesGenericCallableFieldsAndLocalShadowing) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
struct Callable<T> {
    public:
        var function: (T) -> T
        fn apply(value: T) -> T { return function(value) }
        fn apply_with(function: (T) -> T, value: T) -> T { return function(value) }
}
fn increment(value: i64) -> i64 { return value + 1 }
fn check() -> i64 {
    var callable = Callable { function: increment }
    return callable.apply(41) + callable.function(41) + callable.apply_with(increment, 41)
}
)zep"));
}

TEST(FrontendInference, UsesConcreteArgumentsAndResultContextBeforeNumericLiterals) {
    FrontendHarness harness(R"zep(
fn bound<T: f64>(value: T, lower: T, upper: T) -> T { return value }
fn choose<T: i32>(left: T, right: T) -> T { return left }
fn check() -> void {
    var value: f32 = 0.5
    var bounded = bound(value, -1.0, 1.0)
    var wide = choose(1, 4294967296 as i64)
    var contextual: i64 = choose(1, 2)
}
)zep");

    ASSERT_TRUE(harness.type_check_succeeded());
    EXPECT_EQ(harness.variable("bounded", "check")->type->bit_width(), 32);
    EXPECT_EQ(harness.variable("wide", "check")->type->bit_width(), 64);
    EXPECT_EQ(harness.variable("contextual", "check")->initializer->type->bit_width(), 64);

    EXPECT_TRUE(assert_type_check_error(R"zep(
fn choose<T: f64>(left: T, right: T) -> T { return left }
fn invalid() -> void {
    var left: f32 = 1.0
    var right: f64 = 2.0
    choose(left, right)
}
)zep"));
}

TEST(FrontendInference, InfersDefaultAndNestedLiteralArguments) {
    FrontendHarness harness(R"zep(
struct Box<T> { public: var value: T }
struct Outer<T> { public: var box: Box<T> }
fn main() -> i32 {
    var box = Box { value: 1 }
    var outer = Outer { box: box }
    return outer.box.value
}
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());
    const auto* box = harness.variable("box", "main")->type->as<StructType>();
    ASSERT_NE(box, nullptr);
    EXPECT_TRUE(box->find_field("value")->type->same(harness.sema.primitives.entries.at("i32")));
}

TEST(FrontendInference, InfersFactoriesFromTheirExpectedResult) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
struct Holder<T> { public: var ptr: *mut T }
fn allocate<T>() -> *mut T { return null }
fn make<T>() -> Holder<T> { return Holder { ptr: allocate() } }
fn build() -> Holder<i32> { return make() }
fn nested<T>() -> *mut T { return allocate() }
fn main() -> i32 {
    var ptr: *mut i32 = allocate()
    var holder: Holder<i32> = make()
    var other: *mut i32 = nested()
    return 0
}
)zep"));
}

TEST(FrontendInference, KeepsArgumentBindingsWhenTheExpectedResultConflicts) {
    EXPECT_TRUE(assert_type_check_error(R"zep(
fn identity<T>(value: T) -> T { return value }
fn check() -> boolean { return identity(42) }
)zep"));

    EXPECT_TRUE(assert_type_check_error(R"zep(
fn allocate<T>() -> *mut T { return null }
fn check() -> void { var ptr = allocate() }
)zep"));
}

TEST(FrontendInference, InfersNestedCallArgumentsAndNormalizesNamedArguments) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
struct Box<T> { public: var value: T }
fn read<T>(box: Box<T>) -> T { return box.value }
fn choose<T, U>(first: T, second: U) -> T { return first }
fn main() -> i64 {
    var box = Box<i64> { value: 7 }
    var value = read(box)
    return choose(second: true, first: value)
}
)zep"));
}

TEST(FrontendInference, IgnoresNonParameterNamedFields) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
struct Tag { public: var value: i32 }
struct Tagged<T> { public: var tag: Tag var value: T }
fn main() -> i32 {
    var value = Tagged { tag: Tag { value: 2 }, value: 1 }
    return value.value
}
)zep"));
}

TEST(FrontendInference, MatchesPointersArraysAndFunctionTypes) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
fn from_pointer<T>(value: *T) -> T { return *value }
fn from_array<T>(value: T[2]) -> T { return value[0] }
fn invoke<T>(function: (T) -> T, value: T) -> T { return function(value) }
fn identity(value: i32) -> i32 { return value }
fn main() -> i32 {
    var values: i32[2] = [1, 2]
    var value = from_array(values)
    return invoke(identity, from_pointer(&value))
}
)zep"));
}

TEST(FrontendInference, ReportsConflictingRepeatedParameters) {
    FrontendHarness harness(R"zep(
struct Pair<T> { public: var first: T var second: T }
fn main() -> void { var pair = Pair { first: 1, second: true } }
)zep");
    EXPECT_FALSE(harness.type_check_succeeded());
    ASSERT_FALSE(harness.context.diagnostics.entries.empty());
    EXPECT_NE(harness.context.diagnostics.entries.front().message.find(
                  "conflicting inferred types for generic parameter 'T'"), std::string::npos);
}

TEST(FrontendInference, RejectsConflictingCallsRegardlessOfArgumentOrder) {
    for (const auto* arguments : {"1, true", "true, 1"}) {
        FrontendHarness harness(std::string("fn same<T>(first: T, second: T) -> T { return first }\n") +
                                "fn main() -> void { same(" + arguments + ") }\n");
        EXPECT_FALSE(harness.type_check_succeeded());
        ASSERT_FALSE(harness.context.diagnostics.entries.empty());
        EXPECT_NE(harness.context.diagnostics.entries.front().message.find(
                      "conflicting inferred types for generic parameter 'T'"), std::string::npos);
    }
}

TEST(FrontendInference, ReportsUnresolvedParameters) {
    FrontendHarness harness("struct Empty<T> {}\nfn main() -> void { var value = Empty {} }\n");
    EXPECT_FALSE(harness.type_check_succeeded());
    ASSERT_FALSE(harness.context.diagnostics.entries.empty());
    EXPECT_NE(harness.context.diagnostics.entries.front().message.find(
                  "cannot infer generic parameter 'T'"), std::string::npos);
}

TEST(FrontendInference, FailedOverloadInferenceDoesNotPolluteSuccessfulCandidate) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
fn choose<T>(first: T, second: T) -> i32 { return 1 }
fn choose(first: i32, second: boolean) -> i32 { return 2 }
fn main() -> i32 { return choose(1, true) }
)zep"));
}

TEST(FrontendInference, UsesExpectedNominalArgumentsForLiteralFields) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
struct Box<T> { public: var value: T }
struct Outer<T> { public: var box: Box<T> }
enum Maybe<T> { Some { value: T } None }
fn create() -> Box<i64> { return Box { value: 1 } }
fn main() -> i64 {
    var box: Box<i64> = Box { value: 1 }
    var outer: Outer<i64> = Outer { box: Box { value: 2 } }
    var maybe: Maybe<i64> = Maybe::Some { value: 3 }
    return box.value + outer.box.value + create().value
}
)zep"));
}

TEST(FrontendInference, RejectsExplicitOrTypedFieldConflictsWithContext) {
    for (const auto* initializer : {"Box<i32> { value: 1 }", "Box { value: value }"}) {
        EXPECT_TRUE(assert_type_check_error(
            std::string("struct Box<T> { public: var value: T }\nfn main() -> void { ") +
            "var value: boolean = true var box: Box<i64> = " + initializer + " }\n"));
    }
}

TEST(FrontendInference, PreservesDeclarationIdentityAcrossSameNameParameters) {
    FrontendHarness harness("");
    TypeResolver resolver(harness.sema.types, harness.sema.env, harness.context.diagnostics);
    auto outer_identity = 0;
    auto inner_identity = 0;
    NamedType outer("T", {}, &outer_identity);
    NamedType inner("T", {}, &inner_identity);
    std::vector<GenericParameterType> parameters{
        GenericParameterType("T", nullptr, &inner_identity)};
    GenericInference inference(harness.sema, resolver, parameters);
    inference.match(&outer, harness.sema.primitives.entries.at("boolean"));
    inference.match(&inner, harness.sema.primitives.entries.at("i32"));

    const auto arguments = inference.arguments(Span());
    ASSERT_TRUE(arguments.has_value()) << arguments.error();
    ASSERT_EQ(arguments->size(), 1U);
    EXPECT_TRUE(arguments->front()->type->source_type->same(harness.sema.primitives.entries.at("i32")));
    EXPECT_FALSE(harness.context.diagnostics.has_errors());
}

TEST(FrontendInference, DoesNotMatchUnrelatedSameNameNominals) {
    FrontendHarness harness("");
    TypeResolver resolver(harness.sema.types, harness.sema.env, harness.context.diagnostics);
    auto identity = 0;
    NamedType parameter("T", {}, &identity);
    StructType declared("Box", {}, {}, {GenericArgumentType("T", &parameter)});
    StructType actual("Box", {}, {},
                      {GenericArgumentType("T", harness.sema.primitives.entries.at("boolean"))});
    std::vector<GenericParameterType> parameters{GenericParameterType("T", nullptr, &identity)};
    GenericInference inference(harness.sema, resolver, parameters);
    inference.match(&declared, &actual);
    inference.match(&parameter, harness.sema.primitives.entries.at("i32"));

    const auto arguments = inference.arguments(Span());
    ASSERT_TRUE(arguments.has_value()) << arguments.error();
    ASSERT_EQ(arguments->size(), 1U);
    EXPECT_TRUE(arguments->front()->type->source_type->same(harness.sema.primitives.entries.at("i32")));
}

TEST(FrontendInference, ChecksConstraintsAfterInference) {
    EXPECT_TRUE(assert_type_check_error(R"zep(
struct Box<T: i32> { public: var value: T }
fn main() -> void { var box = Box { value: true } }
)zep"));
    EXPECT_TRUE(assert_type_check_error(R"zep(
fn read<T: i32>(value: T) -> T { return value }
fn main() -> void { read(true) }
)zep"));
}

TEST(FrontendInference, AcceptsRepeatedStructurallyEqualFunctionTypes) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
fn first(value: i32) -> i32 { return value }
fn second(value: i32) -> i32 { return value + 1 }
fn choose<T>(first: T, second: T) -> T { return first }
fn main() -> i32 {
    var selected = choose(first, second)
    return selected(1)
}
)zep"));
}

TEST(FrontendInference, UsesUnambiguousCallParameterContext) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
struct Box<T> { public: var value: T }
enum Maybe<T> { Some { value: T } None }
fn consume(box: Box<i64>, maybe: Maybe<i64>) -> i64 { return box.value }
fn main() -> i64 { return consume(maybe: Maybe::None, box: Box { value: 1 }) }
)zep"));
}

TEST(FrontendInference, DoesNotSeedLiteralsFromUnrelatedExpectedNominals) {
    FrontendHarness harness(R"zep(
struct Box<T> { public: var value: T }
struct Other<T> { public: var value: T }
fn main() -> void { var value: Other<i64> = Box { value: 1 } }
)zep");
    EXPECT_FALSE(harness.type_check_succeeded());
    ASSERT_EQ(harness.context.diagnostics.entries.size(), 1U);
    EXPECT_NE(harness.context.diagnostics.entries.front().message.find(
                  "type mismatch in variable declaration"), std::string::npos);
}

TEST(FrontendGlobals, AcceptsRuntimeGlobalInitializers) {
    FrontendHarness harness(R"zep(
fn create() -> i32 { return 1 }
var value = create()
)zep");
    EXPECT_TRUE(harness.type_check_succeeded());
    EXPECT_TRUE(harness.context.diagnostics.entries.empty());
}
