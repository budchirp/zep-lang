#include <benchmark/benchmark.h>
#include <string>

import zep.frontend.test.harness;
import zep.compiler.lowering;

static void BM_lower_sample(benchmark::State& state) {
    std::string source_text = "fn identity<T>(value: T) -> T { return value }\n"
                              "fn main() -> i32 { var mut total: i32 = 0 "
                              "for (var mut i: i32 = 0; i < 100; i = i + 1) { total = total + "
                              "identity<i32>(i) } return total }\n";

    FrontendHarness harness(source_text);
    harness.type_check();

    for (auto _ : state) {
        HIRLowerer lowerer(harness.context, harness.sema);
        auto hir_program = lowerer.lower(harness.program);
        benchmark::DoNotOptimize(hir_program.statements.size());
    }
}

BENCHMARK(BM_lower_sample);
