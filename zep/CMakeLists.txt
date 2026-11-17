cmake_minimum_required(VERSION 3.30)

project(zep
    VERSION 0.1.0
    DESCRIPTION "The Zep programming language compiler"
    LANGUAGES C CXX
)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND DEFINED ENV{HOME})
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local/share/zep" CACHE PATH "Installation prefix" FORCE)
endif()

get_filename_component(ZEP_REPOSITORY_DIR "${PROJECT_SOURCE_DIR}/.." ABSOLUTE)
set(ZEP_STD_SOURCE_DIR "${ZEP_REPOSITORY_DIR}/std")

include(CTest)
include(cmake/Options.cmake)
include(cmake/Compiler.cmake)
include(cmake/Dependencies.cmake)
include(cmake/Flags.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/Coverage.cmake)
include(cmake/Modules.cmake)

if(BUILD_TESTING)
    include(cmake/Testing.cmake)
endif()

include(cmake/Fuzzing.cmake)
include(cmake/Benchmarking.cmake)

add_subdirectory(common)
add_subdirectory(frontend)
add_subdirectory(workspace)
add_subdirectory(hir)
add_subdirectory(compiler)
add_subdirectory(codegen)
add_subdirectory(builder)
add_subdirectory(cli)
add_subdirectory(lsp)

if(BUILD_TESTING)
    zep_finalize_coverage()
endif()

include(cmake/Install.cmake)
