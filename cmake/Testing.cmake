include(FetchContent)
include(GoogleTest)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

function(zep_add_test TARGET)
    cmake_parse_arguments(ARG "" "" "DEPENDS;SOURCES" ${ARGN})

    if(ARG_SOURCES)
        set(TEST_SOURCES ${ARG_SOURCES})
    else()
        file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp")
    endif()

    if(NOT TEST_SOURCES)
        message(FATAL_ERROR "zep_add_test(${TARGET}) found no test sources")
    endif()

    add_executable(${TARGET} ${TEST_SOURCES})

    target_link_libraries(${TARGET}
        PRIVATE
            GTest::gtest_main
            ${ARG_DEPENDS}
    )

    target_compile_definitions(${TARGET}
        PRIVATE
            ZEP_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
            ZEP_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
            ZEP_PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
            ZEP_PROJECT_BINARY_DIR="${PROJECT_BINARY_DIR}"
    )

    zep_configure_target(${TARGET})

    gtest_discover_tests(${TARGET}
        DISCOVERY_TIMEOUT 60
        PROPERTIES
            TIMEOUT 120
            ENVIRONMENT "ZEP_TEST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR};ZEP_TEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR};ZEP_PROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR};ZEP_PROJECT_BINARY_DIR=${PROJECT_BINARY_DIR};LLVM_PROFILE_FILE=${PROJECT_BINARY_DIR}/coverage/%p.profraw"
    )

    if(ZEP_ENABLE_COVERAGE)
        set_property(GLOBAL APPEND PROPERTY ZEP_COVERAGE_TARGETS ${TARGET})
    endif()
endfunction()
