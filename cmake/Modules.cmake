function(zep_configure_target TARGET)
    zep_set_flags(${TARGET})
    zep_set_sanitizers(${TARGET})
endfunction()

function(zep_add_module TARGET)
    cmake_parse_arguments(ARG "" "" "DEPENDS" ${ARGN})

    file(GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cppm")
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

    add_library(${TARGET} STATIC)

    if(MODULE_SOURCES)
        target_sources(${TARGET} PUBLIC FILE_SET CXX_MODULES FILES ${MODULE_SOURCES})
    endif()

    if(SOURCES)
        target_sources(${TARGET} PRIVATE ${SOURCES})
    endif()

    if(ARG_DEPENDS)
        target_link_libraries(${TARGET} PUBLIC ${ARG_DEPENDS})
    endif()

    zep_configure_target(${TARGET})
endfunction()

function(zep_add_executable TARGET)
    cmake_parse_arguments(ARG "" "" "DEPENDS" ${ARGN})

    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
    file(GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cppm")

    add_executable(${TARGET} ${SOURCES})

    if(MODULE_SOURCES)
        target_sources(${TARGET} PUBLIC FILE_SET CXX_MODULES FILES ${MODULE_SOURCES})
    endif()

    if(ARG_DEPENDS)
        target_link_libraries(${TARGET} PRIVATE ${ARG_DEPENDS})
    endif()

    zep_configure_target(${TARGET})
endfunction()