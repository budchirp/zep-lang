zep_add_module(zep_codegen DEPENDS zep_common zep_frontend zep_hir)
target_link_libraries(zep_codegen PUBLIC LLVM)
