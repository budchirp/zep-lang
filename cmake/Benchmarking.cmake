option(ZEP_BUILD_BENCHMARKS "Build benchmark targets" OFF)

if(ZEP_BUILD_BENCHMARKS)
    include(FetchContent)

    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        benchmark
        URL https://github.com/google/benchmark/archive/refs/tags/v1.9.5.tar.gz
    )

    FetchContent_MakeAvailable(benchmark)

    add_custom_target(zep_benchmarks)
endif()

function(zep_add_benchmark TARGET)
    if(NOT ZEP_BUILD_BENCHMARKS)
        return()
    endif()

    cmake_parse_arguments(ARG "" "" "DEPENDS;SOURCES" ${ARGN})

    if(ARG_SOURCES)
        set(BENCHMARK_SOURCES ${ARG_SOURCES})
    else()
        file(GLOB_RECURSE BENCHMARK_SOURCES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.cpp"
        )
    endif()

    if(NOT BENCHMARK_SOURCES)
        message(FATAL_ERROR "zep_add_benchmark(${TARGET}) found no benchmark sources")
    endif()

    add_executable(${TARGET} EXCLUDE_FROM_ALL ${BENCHMARK_SOURCES})

    target_link_libraries(${TARGET}
        PRIVATE
            benchmark::benchmark_main
            ${ARG_DEPENDS}
    )

    zep_configure_target(${TARGET})

    add_dependencies(zep_benchmarks ${TARGET})
endfunction()
