cmake_minimum_required(VERSION 3.30)

project(zep
    VERSION 0.1.0
    DESCRIPTION "The Zep programming language compiler"
    LANGUAGES C CXX
)

include(GNUInstallDirs)
include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

option(ZEP_USE_CCACHE "Use ccache when available" ON)

if(ZEP_USE_CCACHE)
    find_program(CCACHE ccache)
    if(CCACHE)
        message(STATUS "Using ccache as compiler launcher")

        set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE}")
    endif()
endif()

include(cmake/Standards.cmake)
include(cmake/Flags.cmake)
include(cmake/FastLink.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/Coverage.cmake)
include(cmake/Modules.cmake)

set(ZEP_STD_VERSION "0.0.1" CACHE STRING "Zep standard library version to install")

include(CTest)

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

install(FILES "${PROJECT_SOURCE_DIR}/std/zep.json"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/zep/libs/std/${ZEP_STD_VERSION}"
)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/std/src"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/zep/libs/std/${ZEP_STD_VERSION}"
)
