zep_add_test(zep_compiler_tests
    DEPENDS zep_compiler zep_test_support zep_frontend_test_support
)

zep_add_benchmark(zep_compiler_benchmarks
    DEPENDS zep_compiler zep_frontend zep_hir zep_common
)
