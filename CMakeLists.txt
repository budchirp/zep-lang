cmake_minimum_required(VERSION 3.30)

project(zep
    VERSION 0.1.0
    DESCRIPTION "The Zep programming language compiler"
    LANGUAGES CXX
)

find_program(CCACHE ccache)
if(CCACHE)
    message(STATUS "Using ccache as compiler launcher")
    
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE}")
endif()

include(cmake/Standards.cmake)
include(cmake/Flags.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/Modules.cmake)

add_subdirectory(common)
add_subdirectory(frontend)
add_subdirectory(hir)
add_subdirectory(codegen)
add_subdirectory(driver)
add_subdirectory(cli)

