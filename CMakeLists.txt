cmake_minimum_required(VERSION 3.30)

project(zep
    VERSION 0.1.0
    DESCRIPTION "The Zep programming language compiler"
    LANGUAGES CXX
)

include(cmake/Standards.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/Modules.cmake)

add_subdirectory(common)
add_subdirectory(frontend)
add_subdirectory(sema)
add_subdirectory(lowerer)
add_subdirectory(codegen)
add_subdirectory(compiler)
add_subdirectory(cli)
