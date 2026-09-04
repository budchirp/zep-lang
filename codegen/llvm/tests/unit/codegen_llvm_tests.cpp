#include <expected>
#include <filesystem>
#include <gtest/gtest.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Module.h>
#include <string>

import zep.codegen.llvm.backend;
import zep.codegen.llvm.context;
import zep.codegen.llvm.type;
import zep.codegen.api;
import zep.codegen.test.support;
import zep.common.target;
import zep.hir.program;
import zep.test.support;
import zep.frontend.test.harness;
import zep.compiler.lowering;
import zep.frontend.node;
import zep.frontend.sema.type;

class HirHarness {
  public:
    FrontendHarness frontend;

    explicit HirHarness(std::string content) : frontend(std::move(content)) {
        frontend.type_check();
    }

    std::shared_ptr<const HIRProgram> lower() {
        HIRLowerer lowerer(frontend.context, frontend.sema);
        return lowerer.lower(frontend.program);
    }

    bool succeeded() const { return !frontend.context.diagnostics.has_errors(); }
};

TEST(CodegenLlvm, LowersEvaluatedFloatPayloadWithoutReinterpretingBitsAsDigits) {
    TestWorkspace workspace("llvm_const_float");
    HirHarness harness("public fn value(input: f64) -> f64 { return input }\n");
    ASSERT_TRUE(harness.succeeded());
    auto* function = harness.frontend.program.statements[0]->as<FunctionDeclaration>();
    ASSERT_NE(function, nullptr);
    auto* returned = function->body->statements[0]->as<ReturnStatement>();
    ASSERT_NE(returned, nullptr);
    returned->value->compile_time_value =
        CompileTimeValue(-1.25, harness.frontend.sema.builtin_resolver.primitives.at("f64"));
    auto program = harness.lower();
    LLVMBackend backend;

    auto ir = capture_stderr([&] {
        auto result = backend.generate(
            *program, workspace.file("float.o"),
            CodegenOptions(TargetInfo(), OptimizationLevel::Type::O0, DebugOutput::Type::IR));
        EXPECT_TRUE(result.has_value());
    });

    EXPECT_NE(ir.find("ret double -1.250000e+00"), std::string::npos) << ir;
}

TEST(CodegenLlvm, InitializesTargetContext) {
    LLVMEmissionContext context;
    TargetInfo target(TargetInfo::host_triple());

    context.initialize(target, OptimizationLevel::Type::O2);

    ASSERT_NE(context.module, nullptr);
    ASSERT_NE(context.target_machine, nullptr);
    EXPECT_EQ(context.module->getTargetTriple().str(), target.triple);
}

TEST(CodegenLlvm, EmitsObjectForEmptyProgram) {
    TestWorkspace workspace("llvm_empty_program");
    HIRProgram program;
    LLVMBackend codegen;

    assert_emits_object(codegen, program, workspace, "empty.o", OptimizationLevel::Type::O0);
}

TEST(CodegenLlvm, IrSnapshotForEmptyProgramIsStable) {
    TestWorkspace workspace("llvm_ir_snapshot");
    HIRProgram program;
    LLVMBackend codegen;

    auto ir = capture_stderr([&] {
        auto result = codegen.generate(
            static_cast<const HIRProgram&>(program), workspace.file("empty.o"),
            CodegenOptions(TargetInfo(), OptimizationLevel::Type::O0, DebugOutput::Type::IR));
        EXPECT_TRUE(result.has_value());
    });

    EXPECT_TRUE(Snapshot::matches("fixtures/snapshots/empty_ir.txt", ir));
}

TEST(CodegenLlvm, EmitsObjectWithDifferentOptimizationLevels) {
    TestWorkspace workspace("llvm_opt_levels");
    HIRProgram program;
    LLVMBackend codegen;

    assert_emits_object(codegen, program, workspace, "empty_o0.o", OptimizationLevel::Type::O0);
    assert_emits_object(codegen, program, workspace, "empty_o3.o", OptimizationLevel::Type::O3);
}

TEST(CodegenLlvm, BackendDoesNotLeakBetweenPrograms) {
    TestWorkspace workspace("llvm_backend_isolation");
    HIRProgram first;
    HIRProgram second;
    LLVMBackend backend;

    auto first_result = backend.generate(first, workspace.file("first.o"), CodegenOptions());
    auto second_result = backend.generate(second, workspace.file("second.o"), CodegenOptions());

    ASSERT_TRUE(first_result.has_value());
    ASSERT_TRUE(second_result.has_value());
    EXPECT_TRUE(std::filesystem::exists(workspace.file("first.o")));
    EXPECT_TRUE(std::filesystem::exists(workspace.file("second.o")));
}

TEST(CodegenLlvm, EmitsExternVarWithName) {
    TestWorkspace workspace("llvm_extern_var");

    HirHarness harness("@name(\"custom_errno\")\n"
                       "extern var errno: i32\n"
                       "fn main() -> i32 { return errno }\n");
    ASSERT_TRUE(harness.succeeded());

    auto program = harness.lower();
    LLVMBackend codegen;

    auto ir = capture_stderr([&] {
        auto result = codegen.generate(
            *program, workspace.file("extern_var.o"),
            CodegenOptions(TargetInfo(), OptimizationLevel::Type::O0, DebugOutput::Type::IR));
        EXPECT_TRUE(result.has_value());
    });

    EXPECT_NE(ir.find("@custom_errno = external global i32"), std::string::npos);
}
