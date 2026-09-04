add_subdirectory(support)

zep_add_test(zep_frontend_tests
    DEPENDS zep_frontend zep_common zep_test_support zep_frontend_test_support
)

zep_add_fuzzer(zep_frontend_lexer_fuzzer
    SOURCES fuzz/lexer_fuzzer.cpp
    DEPENDS zep_frontend zep_common zep_frontend_test_support
)

zep_add_fuzzer(zep_frontend_parser_fuzzer
    SOURCES fuzz/parser_fuzzer.cpp
    DEPENDS zep_frontend zep_common zep_frontend_test_support
)

zep_add_benchmark(zep_frontend_benchmarks
    DEPENDS zep_frontend zep_common
)
