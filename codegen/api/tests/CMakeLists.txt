zep_add_test(codegen_api_tests SOURCES unit/codegen_api_tests.cpp
    DEPENDS zep_codegen_api zep_hir zep_common zep_test_support)
