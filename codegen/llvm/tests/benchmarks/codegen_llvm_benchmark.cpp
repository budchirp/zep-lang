#include <benchmark/benchmark.h>
#include <filesystem>
#include <string>

import zep.codegen.llvm.backend;
import zep.codegen.api;
import zep.common.target;
import zep.hir.program;
import zep.test.support;

static void BM_emit_empty_object(benchmark::State& state) {
    TestWorkspace workspace("benchmark_codegen_llvm");
    auto index = std::size_t(0);

    for (auto _ : state) {
        HIRProgram program;
        LLVMBackend codegen;
        auto output = workspace.file("empty_" + std::to_string(index++) + ".o");
        codegen.generate(static_cast<const HIRProgram&>(program), output,
                         CodegenOptions(TargetInfo(), OptimizationLevel::Type::O0));
        benchmark::DoNotOptimize(std::filesystem::file_size(output));
    }
}

BENCHMARK(BM_emit_empty_object);
