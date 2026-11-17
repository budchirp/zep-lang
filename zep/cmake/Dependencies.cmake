include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)

FetchContent_Declare(
    argman
    GIT_REPOSITORY https://github.com/budchirp/argman.git
    GIT_TAG 03797650dc22cb38acff4cc8376c674075408e48
    EXCLUDE_FROM_ALL
)

FetchContent_MakeAvailable(nlohmann_json argman)
