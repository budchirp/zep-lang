#include <gtest/gtest.h>
#include <string>

import zep.frontend.test.harness;
import zep.frontend.node;
import zep.frontend.sema.types;

TEST(FrontendWhen, InfersValuesAcrossReturningArms) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
enum Value { Missing Present { value: i64 } }
fn read(value: Value) -> i32 {
    var number = when (value) {
        Value::Missing -> { return -1 },
        Value::Present { value } -> value,
    }
    var next = if (number < 0) { return -2 } else { number + 1 }
    return next as i32
}
fn speed(value: i32) -> f32 {
    return when (value) { 0 -> 0.5, else -> 1.0, }
}
fn literal() -> f32 {
    return when (1) { 1 -> 0.5, else -> 1.0, }
}
)zep"));
    EXPECT_TRUE(assert_type_check_error(R"zep(
fn invalid(value: i32) -> i32 {
    var result = when (value) {
        0 -> { return 0 },
        1 -> true,
        else -> 42,
    }
    return 0
}
)zep"));
}

TEST(FrontendWhen, AcceptsCompleteBooleanAndEnumCoverage) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
enum Choice { First Second }
fn from_boolean(value: boolean) -> i32 {
    return when (value) { true -> 1, false -> 0, }
}
fn from_enum(value: Choice) -> i32 {
    return when (value) { Choice::First -> 1, Choice::Second -> 2, }
}
)zep"));
}

TEST(FrontendWhen, GuardedPatternsDoNotEstablishCoverage) {
    EXPECT_TRUE(assert_type_check_error(R"zep(
fn read(value: boolean, enabled: boolean) -> i32 {
    return when (value) { true if (enabled) -> 1, false -> 0, }
}
)zep"));
    FrontendHarness harness(R"zep(
enum Choice { First Second Third }
fn read(value: Choice, enabled: boolean) -> i32 {
    return when (value) { Choice::First -> 1, Choice::Second if (enabled) -> 2, }
}
)zep");

    EXPECT_FALSE(harness.type_check_succeeded());
    ASSERT_EQ(harness.context.diagnostics.entries.size(), 1U);
    EXPECT_EQ(harness.context.diagnostics.entries.front().message,
              "when expression requires an else arm; missing variants: Choice::Second, Choice::Third");
}

TEST(FrontendWhen, WarnsForCoveredPatternsAndRedundantElse) {
    FrontendHarness harness(R"zep(
enum Choice { First Second }
fn boolean_value(value: boolean) -> i32 {
    return when (value) { true, true -> 1, true if (value) -> 2, false -> 0, else -> 3, }
}
fn enum_value(value: Choice) -> i32 {
    return when (value) { Choice::First, Choice::First -> 1, Choice::Second -> 2, else -> 3, }
}
)zep");

    EXPECT_TRUE(harness.type_check_succeeded());
    EXPECT_EQ(harness.context.diagnostics.entries.size(), 5U);
}

TEST(FrontendWhen, AllowsRepeatedGuardedPatternsBeforeAnUnguardedPattern) {
    FrontendHarness harness(R"zep(
fn read(value: boolean, enabled: boolean) -> i32 {
    return when (value) {
        true if (enabled) -> 1,
        true if (!enabled) -> 2,
        true -> 3,
        false -> 0,
    }
}
)zep");

    EXPECT_TRUE(harness.type_check_succeeded());
    EXPECT_TRUE(harness.context.diagnostics.entries.empty());
}

TEST(FrontendWhen, RejectsDifferentSpecializationsInPatterns) {
    EXPECT_TRUE(assert_type_check_error(R"zep(
enum Maybe<T> { Some { value: T } None }
type IntegerMaybe = Maybe<i32>
type BooleanMaybe = Maybe<boolean>
fn read(value: IntegerMaybe) -> i32 {
    return when (value) { BooleanMaybe::Some { value } -> 1, else -> 0, }
}
)zep"));
    EXPECT_TRUE(assert_type_check_ok(R"zep(
enum Maybe<T> { Some { value: T } None }
fn read(value: Maybe<i32>) -> i32 {
    return when (value) { Maybe::Some { value } -> value, Maybe::None -> 0, }
}
)zep"));
}

TEST(FrontendEnums, ChecksSharedFieldsAndVoidSubstitution) {
    EXPECT_TRUE(assert_type_check_ok(R"zep(
enum Event<T>(context: T) {
    Ready
    Data { count: i32 }
}
enum Outcome<T> { Ok { value: T } Error { code: i32 } }
fn ready() -> Outcome<void> { return Outcome::Ok }
fn check() -> i32 {
    var event: Event<i32> = Event::Data { context: 7, count: 42 }
    return when (event) {
        Event::Ready -> 0,
        Event::Data { count } -> count + event.context,
    }
}
enum Shared<T>(context: T) { Ready }
fn empty() -> Shared<void> { return Shared::Ready }
)zep"));

    EXPECT_TRUE(assert_type_check_error("enum Event(code: i32, code: i32) { Ready }"));
    EXPECT_TRUE(assert_type_check_error("enum Event(code: i32) { Data { code: i32 } }"));
    EXPECT_TRUE(assert_type_check_error("enum Event(code: i32) : i32 { Ready }"));
    EXPECT_TRUE(assert_type_check_error("enum Event { Data { value: void } }"));
    EXPECT_TRUE(assert_type_check_error("fn invalid() -> void { var value: void }"));
    EXPECT_TRUE(assert_type_check_error(R"zep(
enum Item { Value { number: i32 } }
fn invalid() -> void {
    var item = Item::Value { number: 1 }
    when (item) { Item::Value { number } -> number = 2, }
}
)zep"));
}

TEST(FrontendTypeCheckerPositive, AcceptsEnumPayloadConstructionBackedValuesAndWhenDestructuring) {
    const std::string source = R"zep(
enum Key : i32 {
    A = 1
    B = 2
}

struct SampleColor {
    public:
        var r: i32
}

enum Palette : SampleColor {
    Red = SampleColor { r: 255 }
}

interface HasCode {
    public:
        fn code() -> i32
}

enum Flavor : HasCode {
    Vanilla
    Chocolate

    public:
        override fn code() -> i32 {
            return when (*self) {
                Flavor::Vanilla -> 1,
                else -> 2,
            }
        }
}

enum HttpCode : i32, HasCode {
    Ok = 200
    NotFound = 404

    public:
        override fn code() -> i32 {
            return (*self).value
        }
}

enum Option {
    None
    Some { value: i32 }
}

fn main() -> i32 {
    var item = Option::Some { value: Key::A.value }
    var red = Palette::Red.value
    return when (item) {
        Option::Some { value } -> value + red.r + Flavor::Vanilla.code() + HttpCode::Ok.code(),
        else -> 0,
    }
}
)zep";

    EXPECT_TRUE(assert_type_check_ok(source));
}

TEST(FrontendEnums, EvaluatesTypedImplicitAndExplicitDiscriminants) {
    FrontendHarness harness(R"zep(
enum Signed : i8 { First = -3 Second Third = Second + 4 Fourth }
enum Unsigned : u64 { First = 9223372036854775808 Second }
enum Automatic : u8 { First Second }
enum Payload { None Some { value: i32 } }
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());
    const auto& primitives = harness.sema.primitives.entries;
    const auto* signed_type =
        harness.program.statements[0]->as<EnumDeclaration>()->type->as<EnumType>();
    ASSERT_EQ(signed_type->variants.size(), 4);
    EXPECT_EQ(signed_type->variants[0].discriminant,
              CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, primitives.at("i8"),
                               std::int64_t{-3}));
    EXPECT_EQ(signed_type->variants[1].discriminant,
              CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, primitives.at("i8"),
                               std::int64_t{-2}));
    EXPECT_EQ(signed_type->variants[2].discriminant,
              CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, primitives.at("i8"),
                               std::int64_t{2}));
    EXPECT_EQ(signed_type->variants[3].discriminant,
              CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, primitives.at("i8"),
                               std::int64_t{3}));
    const auto* unsigned_type =
        harness.program.statements[1]->as<EnumDeclaration>()->type->as<EnumType>();
    EXPECT_EQ(unsigned_type->variants[0].discriminant,
              CompileTimeValue(CompileTimeValue::Kind::Type::UnsignedInteger, primitives.at("u64"),
                               9223372036854775808ULL));
    EXPECT_EQ(unsigned_type->variants[1].discriminant,
              CompileTimeValue(CompileTimeValue::Kind::Type::UnsignedInteger, primitives.at("u64"),
                               9223372036854775809ULL));
    const auto* automatic =
        harness.program.statements[2]->as<EnumDeclaration>()->type->as<EnumType>();
    EXPECT_EQ(automatic->variants[0].discriminant,
              CompileTimeValue(CompileTimeValue::Kind::Type::UnsignedInteger, primitives.at("u8"),
                               std::uint64_t{0}));
    EXPECT_EQ(automatic->variants[1].discriminant,
              CompileTimeValue(CompileTimeValue::Kind::Type::UnsignedInteger, primitives.at("u8"),
                               std::uint64_t{1}));
    const auto* payload =
        harness.program.statements[3]->as<EnumDeclaration>()->type->as<EnumType>();
    EXPECT_EQ(payload->variants[0].discriminant,
              CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, primitives.at("i32"),
                               std::int64_t{0}));
    EXPECT_EQ(payload->variants[1].discriminant,
              CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, primitives.at("i32"),
                               std::int64_t{1}));
    ASSERT_EQ(payload->variants[1].fields.size(), 1);
    EXPECT_EQ(payload->variants[1].fields[0].name, "value");
}

TEST(FrontendEnums, EvaluatesEarlierQualifiedVariantsAndConstantProjections) {
    FrontendHarness harness(R"zep(
enum Code : i32 { First = 3 Second = Code::First.value + 1 }
enum Flag { Off On }
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());
    const auto* integer = harness.sema.primitives.entries.at("i32");
    const auto* code = harness.program.statements[0]->as<EnumDeclaration>()->type->as<EnumType>();
    const auto* flag = harness.program.statements[1]->as<EnumDeclaration>()->type->as<EnumType>();
    EXPECT_EQ(
        code->variants[1].discriminant,
        CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, integer, std::int64_t{4}));
    EXPECT_EQ(
        flag->variants[1].discriminant,
        CompileTimeValue(CompileTimeValue::Kind::Type::SignedInteger, integer, std::int64_t{1}));
}

TEST(FrontendEnums, RetainsTypedNonIntegerBackingValues) {
    FrontendHarness harness(R"zep(
enum Names : cstr { First = "first" Second = "second" }
enum Flags : boolean { Off = false On = true }
enum Letters : char { A = 'a' B = 'b' }
)zep");
    ASSERT_TRUE(harness.type_check_succeeded());
    const auto& primitives = harness.sema.primitives.entries;
    const auto* names = harness.program.statements[0]->as<EnumDeclaration>()->type->as<EnumType>();
    EXPECT_EQ(names->variants[1].discriminant,
              CompileTimeValue(CompileTimeValue::Kind::Type::String, primitives.at("cstr"),
                               std::string("second")));
    const auto* flags = harness.program.statements[1]->as<EnumDeclaration>()->type->as<EnumType>();
    EXPECT_EQ(
        flags->variants[0].discriminant,
        CompileTimeValue(CompileTimeValue::Kind::Type::Boolean, primitives.at("boolean"), false));
    const auto* letters =
        harness.program.statements[2]->as<EnumDeclaration>()->type->as<EnumType>();
    EXPECT_EQ(letters->variants[1].discriminant,
              CompileTimeValue(CompileTimeValue::Kind::Type::Char, primitives.at("char"),
                               static_cast<std::uint8_t>('b')));
}

TEST(FrontendEnums, RejectsRangeDuplicateAndMissingDiscriminants) {
    EXPECT_TRUE(assert_type_check_error("enum Code : u8 { TooLarge = 256 }\n"));
    EXPECT_TRUE(assert_type_check_error("enum Code : u8 { Negative = -1 }\n"));
    EXPECT_TRUE(assert_type_check_error("enum Code : u8 { Last = 255 Overflow }\n"));
    EXPECT_TRUE(
        assert_type_check_error("enum Code : u64 { Last = 18446744073709551615 Overflow }\n"));
    EXPECT_TRUE(assert_type_check_error("enum Code : i32 { First = 1 Duplicate = 1 }\n"));
    EXPECT_TRUE(assert_type_check_error("enum Code : cstr { Missing }\n"));
    EXPECT_TRUE(
        assert_type_check_error("enum Code : cstr { First = \"same\" Duplicate = \"same\" }\n"));
    EXPECT_TRUE(assert_type_check_error("enum Code : boolean { Invalid = 1 }\n"));
    EXPECT_TRUE(assert_type_check_error(R"zep(
struct Pair { public: var x: i32 var y: i32 }
enum Code : Pair { First = Pair { x: 1, y: 2 } Duplicate = Pair { y: 2, x: 1 } }
)zep"));
}

TEST(FrontendEnums, RejectsCyclesAndForwardReferencesAtTheReference) {
    FrontendHarness forward("enum Code : i32 { First = Second Second = 1 }\n");
    forward.type_check();
    const auto* declaration = forward.program.statements[0]->as<EnumDeclaration>();
    EXPECT_TRUE(assert_diagnostic(forward, "forward enum discriminant reference to 'Second'",
                                  declaration->variants[0]->value_expression->span));
    const auto* type = declaration->type->as<EnumType>();
    ASSERT_EQ(type->variants.size(), 2);
    EXPECT_FALSE(type->variants[0].discriminant.has_value());
    EXPECT_TRUE(type->variants[1].discriminant.has_value());

    FrontendHarness cycle("enum Code : i32 { First = First }\n");
    cycle.type_check();
    declaration = cycle.program.statements[0]->as<EnumDeclaration>();
    EXPECT_TRUE(assert_diagnostic(cycle, "cyclic enum discriminant reference to 'First'",
                                  declaration->variants[0]->value_expression->span));
}
