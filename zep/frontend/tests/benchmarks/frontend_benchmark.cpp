#include <benchmark/benchmark.h>
#include <string>

import zep.frontend.test.harness;
import zep.frontend.token;

static std::string sample_source() {
    return "struct Point { public: var x: i32 var y: i32 }\n"
           "fn add(a: i32, b: i32) -> i32 { return a + b }\n"
           "fn main() -> i32 { var mut total: i32 = 0 "
           "for (var mut i: i32 = 0; i < 100; i = i + 1) { total = add(total, i) } return total "
           "}\n";
}

static void BM_lex_sample(benchmark::State& state) {
    std::string source = sample_source();

    for (auto _ : state) {
        auto tokens = lex_all(source);
        benchmark::DoNotOptimize(tokens.size());
    }
}

static void BM_parse_sample(benchmark::State& state) {
    std::string source_text = sample_source();

    for (auto _ : state) {
        FrontendHarness harness(source_text);
        benchmark::DoNotOptimize(harness.program.statements.size());
    }
}

BENCHMARK(BM_lex_sample);
BENCHMARK(BM_parse_sample);
