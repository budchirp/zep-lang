zep_add_module(zep_lsp
    DEPENDS zep_common zep_compiler zep_frontend zep_workspace nlohmann_json::nlohmann_json
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
