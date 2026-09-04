#include <expected>
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

import zep.build.artifact;
import zep.build.executor;
import zep.build.link;
import zep.build.plan;
import zep.common.system.command;
import zep.common.system.process;
import zep.codegen.api;
import zep.common.target;
import zep.hir.program;

class FakeBackend final : public CodegenBackend {
  public:
    bool fail = false;
    int calls = 0;

    CodegenFormat::Type format() const override { return CodegenFormat::Type::Object; }

    std::expected<void, std::string> generate(const HIRProgram& program,
                                              const std::filesystem::path& output,
                                              const CodegenOptions& options) override {
        static_cast<void>(program);
        static_cast<void>(options);
        ++calls;

        if (fail) {
            return std::unexpected("codegen failed");
        }

        return {};
    }
};

class FakeProcessRunner final : public ProcessRunner {
  public:
    std::vector<std::string> argv;
    bool fail = false;
    int calls = 0;

    ProcessResult run(const Command& command) override {
        argv = command.arguments;
        ++calls;
        return ProcessResult(ProcessExit(ProcessExit::Kind::Type::Exited, fail ? 1 : 0), "",
                             fail ? "link failed" : "");
    }
};

TEST(BuildLinkPlanner, PreservesObjectAndLinkerArgumentOrder) {
    std::vector<ObjectArtifact> objects;
    objects.emplace_back("first.o");
    objects.emplace_back("second.o");

    auto command =
        LinkPlanner::executable(objects, ExecutableArtifact("app"), TargetInfo("x86_64-linux-gnu"),
                                std::vector<std::string>({"-lm", "-pthread"}));

    EXPECT_EQ(command.arguments,
              std::vector<std::string>({"clang", "-fuse-ld=lld", "--target=x86_64-linux-gnu",
                                        "first.o", "second.o", "-lm", "-pthread", "-o", "app"}));
}

TEST(BuildExecutor, StopsBeforeLinkingWhenCodegenFails) {
    auto program = std::make_shared<const HIRProgram>();
    BuildPlan plan;
    plan.add_compile(program, "unit.o", CodegenOptions());
    plan.add_link(Command({"clang", "unit.o", "-o", "app"}, {}), ExecutableArtifact("app"));

    FakeBackend backend;
    backend.fail = true;
    FakeProcessRunner process_runner;
    auto result = BuildExecutor::execute(plan, backend, process_runner);

    ASSERT_FALSE(result.succeeded());
    ASSERT_EQ(result.failures.size(), 1U);
    EXPECT_EQ(result.failures[0].message, "codegen failed");
    EXPECT_EQ(process_runner.calls, 0);
}

TEST(BuildExecutor, ReturnsArtifactsAfterSuccessfulPlan) {
    auto program = std::make_shared<const HIRProgram>();
    BuildPlan plan;
    plan.add_compile(program, "unit.o", CodegenOptions());
    plan.add_link(Command({"clang", "unit.o", "-o", "app"}, {}), ExecutableArtifact("app"));

    FakeBackend backend;
    FakeProcessRunner process_runner;
    auto result = BuildExecutor::execute(plan, backend, process_runner);

    ASSERT_TRUE(result.succeeded());
    ASSERT_EQ(result.objects.size(), 1U);
    ASSERT_EQ(result.executables.size(), 1U);
    EXPECT_EQ(result.objects[0].path, "unit.o");
    EXPECT_EQ(result.executables[0].path, "app");
    EXPECT_EQ(process_runner.argv, std::vector<std::string>({"clang", "unit.o", "-o", "app"}));
}
