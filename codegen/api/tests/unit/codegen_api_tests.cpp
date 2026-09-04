#include <expected>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

import zep.codegen.api;
import zep.codegen.format;
import zep.codegen.options;

class FakeBackend : public CodegenBackend {
  public:
    CodegenFormat::Type selected_format;
    std::filesystem::path selected_output;

    explicit FakeBackend(CodegenFormat::Type format) : selected_format(format) {}

    CodegenFormat::Type format() const override { return selected_format; }

    std::expected<void, std::string> generate(const HIRProgram&,
                                              const std::filesystem::path& output,
                                              const CodegenOptions&) override {
        selected_output = output;
        return {};
    }
};

TEST(CodegenApi, UsesOneOutputPathForAllFormats) {
    HIRProgram program;
    for (auto format : {CodegenFormat::Type::Object, CodegenFormat::Type::CSource,
                        CodegenFormat::Type::Assembly}) {
        FakeBackend backend(format);
        auto result = backend.generate(program, "out", CodegenOptions());
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(backend.selected_output, "out");
    }
}

TEST(CodegenApi, ParsesOptimizationLevels) {
    EXPECT_EQ(OptimizationLevel::from_int(0), OptimizationLevel::Type::O0);
    EXPECT_EQ(OptimizationLevel::from_int(3), OptimizationLevel::Type::O3);
    EXPECT_FALSE(OptimizationLevel::from_int(4).has_value());
}
