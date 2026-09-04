zep_add_module(zep_workspace DEPENDS zep_common nlohmann_json::nlohmann_json)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
