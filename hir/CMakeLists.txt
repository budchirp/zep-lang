zep_add_module(zep_hir DEPENDS zep_common zep_frontend)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
