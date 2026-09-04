module;

#include <filesystem>
#include <gtest/gtest.h>
#include <string>

export module zep.codegen.test.support;

import zep.codegen.api;
import zep.common.target;
import zep.hir.program;
import zep.test.support;

export void assert_emits_object(CodegenBackend& codegen, HIRProgram& program,
                                TestWorkspace& workspace, const std::filesystem::path& filename,
                                OptimizationLevel::Type optimization_level) {
    auto output = workspace.file(filename);
    auto result =
        codegen.generate(program, output, CodegenOptions(TargetInfo(), optimization_level));

    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(output));
    EXPECT_GT(std::filesystem::file_size(output), 0U);
}
