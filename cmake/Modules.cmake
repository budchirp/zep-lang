function(zep_configure_target TARGET)
    zep_set_flags(${TARGET})
    zep_set_sanitizers(${TARGET})
    if(COMMAND zep_set_coverage)
        zep_set_coverage(${TARGET})
    endif()
endfunction()

function(zep_add_module TARGET)
    cmake_parse_arguments(ARG "" "SOURCE_ROOT" "DEPENDS" ${ARGN})

    if(ARG_SOURCE_ROOT)
        set(MODULE_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/${ARG_SOURCE_ROOT}")
    else()
        set(MODULE_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
    endif()

    file(GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "${MODULE_SOURCE_ROOT}/*.cppm")
    file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${MODULE_SOURCE_ROOT}/*.cpp")

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
