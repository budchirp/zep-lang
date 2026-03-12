function(zep_configure_target target)
    zep_set_compiler_warnings(${target})
    zep_set_sanitizers(${target})
endfunction()

function(zep_add_module target)
    cmake_parse_arguments(ARG "" "" "DEPENDS" ${ARGN})

    file(GLOB_RECURSE _module_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cppm")
    file(GLOB_RECURSE _sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

    add_library(${target} STATIC)

    if(_module_sources)
        target_sources(${target} PUBLIC FILE_SET CXX_MODULES FILES ${_module_sources})
    endif()

    if(_sources)
        target_sources(${target} PRIVATE ${_sources})
    endif()

    if(ARG_DEPENDS)
        target_link_libraries(${target} PUBLIC ${ARG_DEPENDS})
    endif()

    zep_configure_target(${target})
endfunction()
