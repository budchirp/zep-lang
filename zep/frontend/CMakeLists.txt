zep_add_module(zep_frontend DEPENDS zep_common)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
