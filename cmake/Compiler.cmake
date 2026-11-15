set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
endif()

if(ZEP_USE_CCACHE)
    find_program(ZEP_CCACHE_EXECUTABLE NAMES ccache)

    if(ZEP_CCACHE_EXECUTABLE)
        foreach(LANGUAGE IN ITEMS C CXX)
            if(NOT DEFINED CMAKE_${LANGUAGE}_COMPILER_LAUNCHER)
                set(CMAKE_${LANGUAGE}_COMPILER_LAUNCHER "${ZEP_CCACHE_EXECUTABLE}")
            endif()
        endforeach()
    endif()
endif()

if(ZEP_USE_FAST_LINKER AND NOT DEFINED CMAKE_LINKER_TYPE)
    find_program(ZEP_MOLD_EXECUTABLE NAMES mold)

    if(ZEP_MOLD_EXECUTABLE)
        set(CMAKE_LINKER_TYPE MOLD)
    endif()
endif()

if(ZEP_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ZEP_IPO_SUPPORTED OUTPUT ZEP_IPO_ERROR LANGUAGES CXX)

    if(NOT ZEP_IPO_SUPPORTED)
        message(FATAL_ERROR "Interprocedural optimization is unavailable: ${ZEP_IPO_ERROR}")
    endif()
endif()
