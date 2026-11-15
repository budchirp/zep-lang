function(zep_configure_target TARGET)
    zep_set_flags(${TARGET})
    zep_set_sanitizers(${TARGET})
    zep_set_coverage(${TARGET})
endfunction()

function(zep_target_sources TARGET)
    file(GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cppm")
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

    if(MODULE_SOURCES)
        target_sources(${TARGET} PUBLIC FILE_SET CXX_MODULES FILES ${MODULE_SOURCES})
    endif()

    if(SOURCES)
        target_sources(${TARGET} PRIVATE ${SOURCES})
    endif()

    zep_configure_target(${TARGET})
endfunction()

function(zep_add_module TARGET)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "DEPENDS")

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown module arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    add_library(${TARGET} STATIC)
    zep_target_sources(${TARGET})

    if(ARG_DEPENDS)
        target_link_libraries(${TARGET} PUBLIC ${ARG_DEPENDS})
    endif()
endfunction()

function(zep_add_executable TARGET)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "DEPENDS")

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown executable arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    add_executable(${TARGET})
    zep_target_sources(${TARGET})

    if(ARG_DEPENDS)
        target_link_libraries(${TARGET} PRIVATE ${ARG_DEPENDS})
    endif()
endfunction()
