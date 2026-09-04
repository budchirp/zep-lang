zep_add_module(zep_build DEPENDS zep_common zep_workspace zep_compiler zep_codegen_api)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
