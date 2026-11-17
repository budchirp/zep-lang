#include <cstdint>
#include <limits>
#include <gtest/gtest.h>
#include <variant>

import zep.common.diagnostic.collection;
import zep.common.source.span;
import zep.frontend.kind;
import zep.frontend.sema.types;
import zep.frontend.sema.constants.math;

TEST(ConstantMath, RejectsUnsignedOverflowIncludingWideProducts) {
    IntegerType type(true, 64);
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    CompileTimeValue value(CompileTimeValue::Kind::Type::UnsignedInteger, &type, maximum);

    for (const auto operation : {BinaryOperator::Type::Plus, BinaryOperator::Type::Asterisk}) {
        Diagnostics diagnostics;
        const auto result = ConstantMath::binary(diagnostics, operation, value, value, &type, Span());

        EXPECT_FALSE(result.has_value());
        ASSERT_EQ(diagnostics.entries.size(), 1U);
        EXPECT_EQ(diagnostics.entries.front().message, "integer overflow in constant expression");
    }
}

TEST(ConstantMath, PreservesSignedMinimumDivisionAndModuloErrors) {
    IntegerType type(false, 64);
    CompileTimeValue minimum(CompileTimeValue::Kind::Type::SignedInteger, &type,
                             std::numeric_limits<std::int64_t>::min());
    CompileTimeValue negative(CompileTimeValue::Kind::Type::SignedInteger, &type, std::int64_t{-1});

    for (const auto operation : {BinaryOperator::Type::Divide, BinaryOperator::Type::Modulo}) {
        Diagnostics diagnostics;
        const auto result = ConstantMath::binary(diagnostics, operation, minimum, negative,
                                                 &type, Span());

        EXPECT_FALSE(result.has_value());
        ASSERT_EQ(diagnostics.entries.size(), 1U);
        EXPECT_EQ(diagnostics.entries.front().message, "integer overflow in constant expression");
    }
}

TEST(ConstantMath, RejectsZeroDivisorsAndUnsignedUnderflow) {
    IntegerType type(true, 8);
    CompileTimeValue zero(CompileTimeValue::Kind::Type::UnsignedInteger, &type, std::uint64_t{0});
    CompileTimeValue one(CompileTimeValue::Kind::Type::UnsignedInteger, &type, std::uint64_t{1});

    for (const auto operation : {BinaryOperator::Type::Divide, BinaryOperator::Type::Modulo,
                                 BinaryOperator::Type::Minus}) {
        Diagnostics diagnostics;

        EXPECT_FALSE(ConstantMath::binary(diagnostics, operation, zero,
            operation == BinaryOperator::Type::Minus ? one : zero, &type, Span()).has_value());
        EXPECT_TRUE(diagnostics.has_errors());
    }
}

TEST(ConstantMath, ChecksNarrowAndCharacterCastsWithOneRangePolicy) {
    Diagnostics diagnostics;
    IntegerType source(true, 64);
    IntegerType target(true, 8);
    CharType character;
    CompileTimeValue maximum(CompileTimeValue::Kind::Type::UnsignedInteger, &source, std::uint64_t{255});
    CompileTimeValue overflow(CompileTimeValue::Kind::Type::UnsignedInteger, &source, std::uint64_t{256});

    EXPECT_TRUE(ConstantMath::cast(diagnostics, maximum, &target, Span()).has_value());
    EXPECT_TRUE(ConstantMath::cast(diagnostics, maximum, &character, Span()).has_value());
    EXPECT_FALSE(ConstantMath::cast(diagnostics, overflow, &target, Span()).has_value());
    EXPECT_FALSE(ConstantMath::cast(diagnostics, overflow, &character, Span()).has_value());
}

TEST(ConstantMath, ComparesLargeUnsignedValuesWithoutSignedNarrowing) {
    Diagnostics diagnostics;
    IntegerType type(true, 64);
    BooleanType boolean;
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    CompileTimeValue left(CompileTimeValue::Kind::Type::UnsignedInteger, &type, maximum);
    CompileTimeValue right(CompileTimeValue::Kind::Type::UnsignedInteger, &type, maximum - 1);

    const auto result = ConstantMath::binary(diagnostics, BinaryOperator::Type::GreaterThan,
                                             left, right, &boolean, Span());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::get<bool>(result->payload));
    EXPECT_FALSE(diagnostics.has_errors());
}
