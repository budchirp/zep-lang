#include <expected>
#include <filesystem>
#include <gtest/gtest.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Module.h>
#include <string>

import zep.codegen.llvm.backend;
import zep.codegen.llvm.target_context;
import zep.codegen.llvm.types;
import zep.codegen.api;
import zep.common.target;
import zep.lowerer.nodes;
import zep.test.support;
import zep.frontend.test.harness;
import zep.lowerer;
import zep.frontend.node;
import zep.frontend.sema.types;
import zep.compiler.test.harness;

TEST(CodegenLlvm, LowersEvaluatedFloatPayloadWithoutReinterpretingBitsAsDigits) {
    TestWorkspace workspace("llvm_const_float");
    HirHarness harness("public fn value(input: f64) -> f64 { return input }\n");
    ASSERT_TRUE(harness.succeeded());
    auto* function = harness.frontend.program.statements[0]->as<FunctionDeclaration>();
    ASSERT_NE(function, nullptr);
    auto* returned = function->body->statements[0]->as<ReturnStatement>();
    ASSERT_NE(returned, nullptr);
    returned->value->compile_time_value =
        CompileTimeValue(-1.25, harness.frontend.sema.primitives.entries.at("f64"));
    auto program = harness.lower();
    LLVMBackend backend;

    auto ir = capture_stderr([&] {
        auto result = backend.generate(
            *program, workspace.file("float.o"),
            EmitOptions(TargetInfo(), OptimizationLevel::Type::O0, DebugOutput::Type::IR));
        EXPECT_TRUE(result.has_value());
    });

    EXPECT_NE(ir.find("ret double -1.250000e+00"), std::string::npos) << ir;
}

TEST(CodegenLlvm, EmitsReleaseAfterExplicitDestruction) {
    TestWorkspace workspace("llvm_release");
    HirHarness harness(R"zep(
struct Memory {
    public:
        static fn free(ptr: *mut void) -> void {}
}
struct Record {
    public:
        var value: i32
        fn ~Record() -> void {}
}
public fn release(ptr: *mut Record) -> void {
    if (ptr != null) {
        ptr[0].~Record()
        Memory::free(ptr as *mut void)
    }
}
)zep");
    ASSERT_TRUE(harness.succeeded());
    const auto program = harness.lower();
    ASSERT_NE(program, nullptr);
    LLVMBackend backend;

    const auto ir = capture_stderr([&] {
        const auto result = backend.generate(*program, workspace.file("release.o"),
            EmitOptions(TargetInfo(), OptimizationLevel::Type::O0, DebugOutput::Type::IR));
        EXPECT_TRUE(result.has_value());
    });

    const auto destructor = ir.find("call void @\"Record::~Record");
    const auto release = ir.find("call void @\"Memory::free");

    ASSERT_NE(destructor, std::string::npos) << ir;
    ASSERT_NE(release, std::string::npos) << ir;
    EXPECT_LT(destructor, release);
}

TEST(CodegenLlvm, CopiesEmptyBasesWithoutMemoryAccess) {
    TestWorkspace workspace("llvm_empty_base");
    HirHarness harness(R"zep(
struct Base {}
struct Record : Base { public: var value: i32 }
public fn copy(source: &Record) -> Record { return source }
)zep");
    ASSERT_TRUE(harness.succeeded());

    auto program = harness.lower();
    LLVMBackend backend;

    const auto ir = capture_stderr([&] {
        const auto result = backend.generate(*program, workspace.file("copy.o"),
            EmitOptions(TargetInfo(), OptimizationLevel::Type::O0, DebugOutput::Type::IR));

        EXPECT_TRUE(result.has_value());
    });

    EXPECT_EQ(ir.find("load %Base"), std::string::npos) << ir;
    EXPECT_EQ(ir.find("store %Base"), std::string::npos) << ir;
}

TEST(CodegenLlvm, InitializesTargetContext) {
    Target context;
    TargetInfo target(TargetInfo::host_triple());

    const auto result = context.initialize(target, OptimizationLevel::Type::O2);

    ASSERT_TRUE(result.has_value()) << result.error();
    ASSERT_NE(context.module, nullptr);
    ASSERT_NE(context.target_machine, nullptr);
    EXPECT_EQ(context.module->getTargetTriple().str(), target.triple);
}

TEST(CodegenLlvm, ReportsAnUnknownTargetWithoutThrowing) {
    Target context;
    const auto result = context.initialize(TargetInfo("not-a-target"));

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("could not find target"), std::string::npos);
}

TEST(CodegenLlvm, ReportsMissingCallMetadataThroughItsFailureResult) {
    TestWorkspace workspace("llvm_missing_call_metadata");
    HirHarness harness("fn work() -> void {} fn main() -> void { work() }");
    ASSERT_TRUE(harness.succeeded());

    auto program = harness.lower();
    ASSERT_NE(program, nullptr);
    auto* function = HirHarness::function(*program, "main");
    ASSERT_NE(function, nullptr);
    auto* statement = function->body->statements.front()->as<HIRExpressionStatement>();
    ASSERT_NE(statement, nullptr);
    auto* call = statement->expression->as<HIRCallExpression>();
    ASSERT_NE(call, nullptr);
    auto* target = call->target->as<HIRDirectCallTarget>();
    ASSERT_NE(target, nullptr);
    target->function_type = nullptr;

    LLVMBackend backend;
    const auto result = backend.generate(*program, workspace.file("invalid.o"), EmitOptions());

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "resolved call has no emitted callee or signature");
    EXPECT_FALSE(std::filesystem::exists(workspace.file("invalid.o")));
}

TEST(CodegenLlvm, KeepsSameNamedTypesDistinctAndReusesEquivalentInstances) {
    llvm::LLVMContext context;
    Types types(context);
    IntegerType narrow(false, 32);
    IntegerType wide(false, 64);
    StructType first("Failure", {}, {FieldType("value", &narrow)});
    StructType second("Failure", {}, {FieldType("value", &wide)});
    StructType equivalent("Failure", {}, {FieldType("value", &narrow)}, {}, nullptr, {}, {}, &first);

    auto* first_layout = llvm::cast<llvm::StructType>(types.lower(&first));
    auto* second_layout = llvm::cast<llvm::StructType>(types.lower(&second));

    EXPECT_NE(first_layout, second_layout);
    EXPECT_TRUE(first_layout->getElementType(0)->isIntegerTy(32));
    EXPECT_TRUE(second_layout->getElementType(0)->isIntegerTy(64));
    EXPECT_EQ(types.lower(&equivalent), first_layout);
}

TEST(CodegenLlvm, IrSnapshotForEmptyProgramIsStable) {
    TestWorkspace workspace("llvm_ir_snapshot");
    HIRProgram program;
    LLVMBackend codegen;

    auto ir = capture_stderr([&] {
        auto result = codegen.generate(
            static_cast<const HIRProgram&>(program), workspace.file("empty.o"),
            EmitOptions(TargetInfo(), OptimizationLevel::Type::O0, DebugOutput::Type::IR));
        EXPECT_TRUE(result.has_value());
    });

    EXPECT_TRUE(Snapshot::matches("fixtures/snapshots/empty_ir.txt", ir));
}

TEST(CodegenLlvm, EmitsIndependentProgramsAtDifferentOptimizationLevels) {
    TestWorkspace workspace("llvm_opt_levels");
    LLVMBackend codegen;

    for (const auto level : {OptimizationLevel::Type::O0, OptimizationLevel::Type::O3}) {
        HIRProgram program;
        const auto output = workspace.file(level == OptimizationLevel::Type::O0 ? "empty_o0.o" : "empty_o3.o");
        const auto result = codegen.generate(program, output, EmitOptions(TargetInfo(), level));

        ASSERT_TRUE(result.has_value()) << result.error();
        EXPECT_TRUE(std::filesystem::is_regular_file(output));
        EXPECT_GT(std::filesystem::file_size(output), 0U);
    }
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
            EmitOptions(TargetInfo(), OptimizationLevel::Type::O0, DebugOutput::Type::IR));
        EXPECT_TRUE(result.has_value());
    });

    EXPECT_NE(ir.find("@custom_errno = external global i32"), std::string::npos);
}

TEST(CodegenLlvm, EmitsUnterminatedConditionalLoopAndShortCircuitBlocks) {
    TestWorkspace workspace("llvm_control_flow_terminators");
    HirHarness harness(R"zep(
fn main() -> i32 {
    var mut value: i32 = 0
    while (value < 3) {
        value = value + 1
    }
    return if (value == 3 && true) { value } else { 0 }
}
)zep");
    ASSERT_TRUE(harness.succeeded());

    auto program = harness.lower();
    ASSERT_NE(program, nullptr);
    LLVMBackend backend;

    auto result = backend.generate(*program, workspace.file("control_flow.o"), EmitOptions());

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(std::filesystem::exists(workspace.file("control_flow.o")));
}
