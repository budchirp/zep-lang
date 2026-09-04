option(ZEP_ENABLE_COVERAGE "Enable LLVM source-based coverage" OFF)

if(ZEP_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "ZEP_ENABLE_COVERAGE requires Clang")
    endif()

    find_program(ZEP_LLVM_PROFDATA llvm-profdata REQUIRED)
    find_program(ZEP_LLVM_COV llvm-cov REQUIRED)

    add_custom_target(zep_coverage_tests)
endif()

function(zep_set_coverage TARGET)
    if(NOT ZEP_ENABLE_COVERAGE)
        return()
    endif()

    target_compile_options(${TARGET}
        PRIVATE
            -fprofile-instr-generate
            -fcoverage-mapping
    )

    target_link_options(${TARGET}
        PRIVATE
            -fprofile-instr-generate
            -fcoverage-mapping
    )
endfunction()

function(zep_finalize_coverage)
    if(NOT ZEP_ENABLE_COVERAGE)
        return()
    endif()

    get_property(COVERAGE_TARGETS GLOBAL PROPERTY ZEP_COVERAGE_TARGETS)
    if(NOT COVERAGE_TARGETS)
        message(FATAL_ERROR "ZEP_ENABLE_COVERAGE requires at least one zep_add_test target")
    endif()

    foreach(TARGET IN LISTS COVERAGE_TARGETS)
        add_dependencies(zep_coverage_tests ${TARGET})
    endforeach()

    list(GET COVERAGE_TARGETS 0 FIRST_TARGET)
    set(COVERAGE_OBJECTS "$<TARGET_FILE:${FIRST_TARGET}>")

    list(REMOVE_AT COVERAGE_TARGETS 0)
    foreach(TARGET IN LISTS COVERAGE_TARGETS)
        list(APPEND COVERAGE_OBJECTS -object "$<TARGET_FILE:${TARGET}>")
    endforeach()

    add_custom_target(zep_coverage
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${PROJECT_BINARY_DIR}/coverage"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/coverage"
        COMMAND ${CMAKE_COMMAND} -E env
            "LLVM_PROFILE_FILE=${PROJECT_BINARY_DIR}/coverage/%p.profraw"
            ${CMAKE_CTEST_COMMAND} --test-dir "${PROJECT_BINARY_DIR}" --output-on-failure
        COMMAND ${CMAKE_COMMAND}
            -DZEP_COVERAGE_DIR="${PROJECT_BINARY_DIR}/coverage"
            -DZEP_PROFDATA="${PROJECT_BINARY_DIR}/coverage/zep.profdata"
            -DZEP_LLVM_PROFDATA="${ZEP_LLVM_PROFDATA}"
            -P "${PROJECT_SOURCE_DIR}/cmake/RunCoverageMerge.cmake"
        COMMAND ${ZEP_LLVM_COV} report
            ${COVERAGE_OBJECTS}
            -instr-profile="${PROJECT_BINARY_DIR}/coverage/zep.profdata"
            -ignore-filename-regex=_deps
            -ignore-filename-regex=cmake-build
            -ignore-filename-regex=/tests/
            -ignore-filename-regex=examples/pong/third_party
        DEPENDS zep_coverage_tests
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    )
endfunction()
