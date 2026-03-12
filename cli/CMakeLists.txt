include(FetchContent)

FetchContent_Declare(
    argman
    GIT_REPOSITORY https://github.com/budchirp/argman.git
    GIT_TAG main
)

FetchContent_MakeAvailable(argman)

zep_add_executable(zep DEPENDS zep_compiler argman)