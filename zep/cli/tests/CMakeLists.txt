add_subdirectory(support)

zep_add_test(zep_cli_zep_tests
    DEPENDS zep_test_support zep_cli_test_support
)

target_compile_definitions(zep_cli_zep_tests
    PRIVATE
        ZEP_ZEP_EXECUTABLE="$<TARGET_FILE:zep>"
)

add_dependencies(zep_cli_zep_tests zep)
