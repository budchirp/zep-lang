#include <gtest/gtest.h>

import zep.codegen.options;

TEST(CodegenApi, ParsesOptimizationLevels) {
    EXPECT_EQ(OptimizationLevel::from_int(0), OptimizationLevel::Type::O0);
    EXPECT_EQ(OptimizationLevel::from_int(1), OptimizationLevel::Type::O1);
    EXPECT_EQ(OptimizationLevel::from_int(2), OptimizationLevel::Type::O2);
    EXPECT_EQ(OptimizationLevel::from_int(3), OptimizationLevel::Type::O3);
    EXPECT_FALSE(OptimizationLevel::from_int(-1).has_value());
    EXPECT_FALSE(OptimizationLevel::from_int(4).has_value());
}
