zep_add_module(zep_codegen_api DEPENDS zep_common zep_hir)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
