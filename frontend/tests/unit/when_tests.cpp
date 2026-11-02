#include <gtest/gtest.h>
#include <string>

import zep.frontend.test.harness;

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
