add_subdirectory(support)

zep_add_test(zep_common_tests
    DEPENDS zep_common zep_test_support nlohmann_json::nlohmann_json
)
