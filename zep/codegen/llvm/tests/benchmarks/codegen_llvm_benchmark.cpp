#include <benchmark/benchmark.h>
#include <filesystem>

import zep.codegen.llvm.backend;
import zep.codegen.api;
import zep.common.target;
import zep.hir.program;
import zep.test.support;

static void BM_emit_empty_object(benchmark::State& state) {
    TestWorkspace workspace("benchmark_codegen_llvm");
    const auto output = workspace.file("empty.o");

    for (auto _ : state) {
        HIRProgram program;
        LLVMBackend codegen;
        const auto result = codegen.generate(program, output,
            EmitOptions(TargetInfo(), OptimizationLevel::Type::O0));

        if (!result.has_value()) {
            state.SkipWithError(result.error().c_str());

            break;
        }

        benchmark::DoNotOptimize(std::filesystem::file_size(output));
    }
}

BENCHMARK(BM_emit_empty_object);
