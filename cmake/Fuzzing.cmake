option(ZEP_BUILD_FUZZERS "Build libFuzzer targets" OFF)

if(ZEP_BUILD_FUZZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "ZEP_BUILD_FUZZERS requires Clang")
    endif()

    add_custom_target(zep_fuzzers)
    add_custom_target(zep_fuzz_smoke)
endif()

function(zep_add_fuzzer TARGET)
    if(NOT ZEP_BUILD_FUZZERS)
        return()
    endif()

    cmake_parse_arguments(ARG "" "" "DEPENDS;SOURCES" ${ARGN})

    if(ARG_SOURCES)
        set(FUZZ_SOURCES ${ARG_SOURCES})
    else()
        set(FUZZ_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/${TARGET}.cpp")
    endif()

    add_executable(${TARGET} EXCLUDE_FROM_ALL ${FUZZ_SOURCES})

    target_link_libraries(${TARGET}
        PRIVATE
            ${ARG_DEPENDS}
    )

    target_compile_options(${TARGET}
        PRIVATE
            -fsanitize=fuzzer,address,undefined
            -fno-omit-frame-pointer
    )

    target_link_options(${TARGET}
        PRIVATE
            -fsanitize=fuzzer,address,undefined
    )

    zep_configure_target(${TARGET})

    add_custom_target(${TARGET}_smoke
        COMMAND $<TARGET_FILE:${TARGET}> -runs=1
        DEPENDS ${TARGET}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    add_dependencies(zep_fuzzers ${TARGET})
    add_dependencies(zep_fuzz_smoke ${TARGET}_smoke)
endfunction()
