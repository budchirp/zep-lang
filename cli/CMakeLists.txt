include(FetchContent)

FetchContent_Declare(
    argman
    GIT_REPOSITORY https://github.com/budchirp/argman.git
    GIT_TAG main
)

FetchContent_MakeAvailable(argman)

file(GLOB_RECURSE ZEP_CLI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE ZEP_CLI_MODULE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cppm")

add_executable(zep ${ZEP_CLI_SOURCES})

if(ZEP_CLI_MODULE_SOURCES)
    target_sources(zep PUBLIC FILE_SET CXX_MODULES FILES ${ZEP_CLI_MODULE_SOURCES})
endif()

target_link_libraries(zep PRIVATE zep_compiler argman)
zep_configure_target(zep)
