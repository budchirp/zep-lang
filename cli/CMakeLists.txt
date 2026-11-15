zep_add_executable(zep DEPENDS zep_build zep_codegen_llvm zep_lsp argman)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

target_compile_definitions(zep
    PRIVATE
        ZEP_STD_SOURCE_DIR="${PROJECT_SOURCE_DIR}/std"
        ZEP_STD_VERSION="${ZEP_STD_VERSION}"
)
