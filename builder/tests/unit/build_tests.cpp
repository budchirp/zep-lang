#include <algorithm>
#include <expected>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <vector>

import zep.build;
import zep.codegen.api;
import zep.common.system.command;
import zep.common.system.process;
import zep.hir.program;
import zep.test.support;
import zep.workspace.toolchain;

class FakeBackend final : public CodegenBackend {
  public:
    bool fail = false;
    std::vector<std::filesystem::path> outputs;
    std::vector<std::string> targets;

    CodegenFormat::Type format() const override { return CodegenFormat::Type::Object; }

    std::expected<void, std::string> generate(const HIRProgram& program,
                                              const std::filesystem::path& output,
                                              const CodegenOptions& options) override {
        static_cast<void>(program);
        outputs.push_back(output);
        targets.push_back(options.target.triple);
        if (fail) {
            return std::unexpected("codegen failed");
        }
        return {};
    }
};

class FakeProcessRunner final : public ProcessRunner {
  public:
    bool fail = false;
    std::vector<Command> commands;

    ProcessResult run(const Command& command) override {
        commands.push_back(command);
        return ProcessResult(ProcessExit(ProcessExit::Kind::Type::Exited, fail ? 1 : 0), "",
                             fail ? "link failed" : "");
    }
};

TEST(Builder, GeneratesObjectsInModuleOrderAndLinksWithTarget) {
    TestWorkspace workspace("builder_objects");
    workspace.write("zep.json", R"({
        "name": "builder_objects",
        "target": [{"triple": "x86_64-unknown-linux-gnu",
                    "linker": {"arguments": ["-lm"]}}]
    })");
    workspace.write("src/helper.zep", "public fn value() -> i32 { return 1 }");
    workspace.write("src/main.zep", "import helper.value\nfn main() -> i32 { return value() }");

    FakeBackend backend;
    FakeProcessRunner process_runner;
    Builder builder(backend, Toolchain("clang", {}, {}), process_runner);

    ASSERT_TRUE(
        builder.build(workspace.root(), OptimizationLevel::Type::O0, DebugOutput::Type::None));
    ASSERT_EQ(backend.outputs.size(), 2U);
    ASSERT_EQ(process_runner.commands.size(), 1U);
    EXPECT_EQ(backend.targets,
              std::vector<std::string>({"x86_64-unknown-linux-gnu", "x86_64-unknown-linux-gnu"}));
    const auto& arguments = process_runner.commands.front().arguments;
    EXPECT_EQ(arguments.front(), "clang");
    EXPECT_NE(std::ranges::find(arguments, "-lm"), arguments.end());
    EXPECT_NE(std::ranges::find(arguments, "--target=x86_64-unknown-linux-gnu"), arguments.end());
}

TEST(Builder, StopsBeforeLinkingWhenCodeGenerationFails) {
    TestWorkspace workspace("builder_codegen_failure");
    workspace.write("zep.json", R"({"name": "builder_codegen_failure"})");
    workspace.write("src/main.zep", "fn main() -> i32 { return 0 }");

    FakeBackend backend;
    backend.fail = true;
    FakeProcessRunner process_runner;
    Builder builder(backend, Toolchain("clang", {}, {}), process_runner);

    EXPECT_FALSE(
        builder.build(workspace.root(), OptimizationLevel::Type::O0, DebugOutput::Type::None));
    EXPECT_TRUE(process_runner.commands.empty());
}

TEST(Builder, DoesNotLinkLibraries) {
    TestWorkspace workspace("builder_library");
    workspace.write("zep.json", R"({"name": "builder_library", "type": "library"})");
    workspace.write("src/lib.zep", "public fn value() -> i32 { return 1 }");

    FakeBackend backend;
    FakeProcessRunner process_runner;
    Builder builder(backend, Toolchain("clang", {}, {}), process_runner);

    EXPECT_TRUE(
        builder.build(workspace.root(), OptimizationLevel::Type::O0, DebugOutput::Type::None));
    EXPECT_EQ(backend.outputs.size(), 1U);
    EXPECT_TRUE(process_runner.commands.empty());
}

TEST(Builder, PropagatesLinkFailure) {
    TestWorkspace workspace("builder_link_failure");
    workspace.write("zep.json", R"({"name": "builder_link_failure"})");
    workspace.write("src/main.zep", "fn main() -> i32 { return 0 }");

    FakeBackend backend;
    FakeProcessRunner process_runner;
    process_runner.fail = true;
    Builder builder(backend, Toolchain("clang", {}, {}), process_runner);

    EXPECT_FALSE(
        builder.build(workspace.root(), OptimizationLevel::Type::O0, DebugOutput::Type::None));
    EXPECT_EQ(process_runner.commands.size(), 1U);
}

TEST(Builder, UsesEachTargetForAnalysisGenerationAndLinking) {
    TestWorkspace workspace("builder_targets");
    workspace.write("zep.json", R"({
        "name": "builder_targets",
        "target": [
            {"triple": "x86_64-unknown-linux-gnu"},
            {"triple": "aarch64-unknown-linux-gnu"}
        ]
    })");
    workspace.write("src/main.zep", "fn main() -> i32 { return 0 }");

    FakeBackend backend;
    FakeProcessRunner process_runner;
    Builder builder(backend, Toolchain("clang", {}, {}), process_runner);

    ASSERT_TRUE(
        builder.build(workspace.root(), OptimizationLevel::Type::O0, DebugOutput::Type::None));
    EXPECT_EQ(backend.targets,
              std::vector<std::string>({"x86_64-unknown-linux-gnu", "aarch64-unknown-linux-gnu"}));
    ASSERT_EQ(process_runner.commands.size(), 2U);
    EXPECT_NE(std::ranges::find(process_runner.commands[0].arguments,
                                "--target=x86_64-unknown-linux-gnu"),
              process_runner.commands[0].arguments.end());
    EXPECT_NE(std::ranges::find(process_runner.commands[1].arguments,
                                "--target=aarch64-unknown-linux-gnu"),
              process_runner.commands[1].arguments.end());
}
