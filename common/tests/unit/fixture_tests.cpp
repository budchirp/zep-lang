#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

import zep.test.support;

TEST(TestFixtures, ExcludesGeneratedBuildDirectories) {
    TestWorkspace source("fixture_copy_source");
    TestWorkspace destination("fixture_copy_destination");
    source.write("sample/src/main.zep", "fn main() -> i32 { return 0 }");
    source.write("sample/zep.json", "{}");
    source.write("sample/build/objs/main.o", "stale");
    source.write("sample/nested/build/executable", "stale");

    const auto* previous = std::getenv("ZEP_TEST_SOURCE_DIR");
    auto previous_root = previous != nullptr ? std::string(previous) : std::string();
    ASSERT_EQ(setenv("ZEP_TEST_SOURCE_DIR", source.root().c_str(), 1), 0);

    destination.copy_fixture("sample");

    if (previous != nullptr) {
        EXPECT_EQ(setenv("ZEP_TEST_SOURCE_DIR", previous_root.c_str(), 1), 0);
    } else {
        EXPECT_EQ(unsetenv("ZEP_TEST_SOURCE_DIR"), 0);
    }

    EXPECT_EQ(TextFile::read(destination.file("sample/src/main.zep")),
              "fn main() -> i32 { return 0 }");
    EXPECT_TRUE(std::filesystem::exists(destination.file("sample/zep.json")));
    EXPECT_FALSE(std::filesystem::exists(destination.file("sample/build")));
    EXPECT_FALSE(std::filesystem::exists(destination.file("sample/nested/build")));
}
