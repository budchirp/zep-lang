zep_add_executable(zep DEPENDS zep_compiler zep_codegen_llvm zep_lsp argman)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

target_compile_definitions(zep
    PRIVATE
        ZEP_STD_SOURCE_DIR="${ZEP_STD_SOURCE_DIR}"
)
