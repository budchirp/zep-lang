#include <gtest/gtest.h>
#include <string>

import zep.frontend.test.harness;
import zep.frontend.node;
import zep.frontend.sema.type;

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
    const auto& primitives = harness.sema.builtin_resolver.primitives;
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
    const auto* integer = harness.sema.builtin_resolver.primitives.at("i32");
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
    const auto& primitives = harness.sema.builtin_resolver.primitives;
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
