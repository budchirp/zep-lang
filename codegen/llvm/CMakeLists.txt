find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

zep_add_module(zep_codegen_llvm DEPENDS zep_common zep_frontend zep_hir zep_codegen_common)
target_link_libraries(zep_codegen_llvm PUBLIC LLVM)
