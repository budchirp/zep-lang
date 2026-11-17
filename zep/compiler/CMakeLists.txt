zep_add_module(zep_compiler DEPENDS zep_common zep_workspace zep_frontend zep_lowerer zep_codegen_api)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
