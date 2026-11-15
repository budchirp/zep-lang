#include <benchmark/benchmark.h>
#include <string>

import zep.compiler.test.harness;

static void BM_lower_sample(benchmark::State& state) {
    std::string source_text = "fn identity<T>(value: T) -> T { return value }\n"
                              "fn main() -> i32 { var mut total: i32 = 0 "
                              "for (var mut i: i32 = 0; i < 100; i = i + 1) { total = total + "
                              "identity<i32>(i) } return total }\n";

    HirHarness harness(source_text);

    if (!harness.succeeded()) {
        state.SkipWithError("semantic checking failed");

        return;
    }

    for (auto _ : state) {
        const auto program = harness.lower();

        if (program == nullptr) {
            state.SkipWithError("lowering failed");

            break;
        }

        benchmark::DoNotOptimize(program->statements.size());
    }
}

BENCHMARK(BM_lower_sample);
