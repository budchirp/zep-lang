file(GLOB_RECURSE MODULE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cppm")

add_library(zep_lsp STATIC)
target_sources(zep_lsp PUBLIC FILE_SET CXX_MODULES FILES ${MODULE_SOURCES})
target_link_libraries(zep_lsp PUBLIC zep_compiler zep_workspace zep_frontend zep_common
                                       nlohmann_json::nlohmann_json)
zep_configure_target(zep_lsp)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
