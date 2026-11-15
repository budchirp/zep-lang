zep_add_test(zep_codegen_llvm_tests
    DEPENDS zep_codegen_llvm zep_test_support zep_compiler_test_support
)

zep_add_benchmark(zep_codegen_llvm_benchmarks
    DEPENDS zep_codegen_llvm zep_test_support
)
