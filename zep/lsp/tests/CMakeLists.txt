zep_add_test(zep_lsp_tests
    DEPENDS zep_lsp zep_test_support nlohmann_json::nlohmann_json
)

target_compile_definitions(zep_lsp_tests
    PRIVATE
        ZEP_ZEP_EXECUTABLE="$<TARGET_FILE:zep>"
)

add_dependencies(zep_lsp_tests zep)
