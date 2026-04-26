add_subdirectory(common)
add_subdirectory(llvm)

zep_add_module(zep_codegen DEPENDS zep_common zep_hir zep_codegen_common zep_codegen_llvm)
