install(TARGETS zep
    RUNTIME DESTINATION bin COMPONENT Compiler
)

install(FILES "${ZEP_STD_SOURCE_DIR}/zep.json"
    DESTINATION "libs/std/${ZEP_STD_VERSION}"
    COMPONENT StandardLibrary
)

install(DIRECTORY "${ZEP_STD_SOURCE_DIR}/src"
    DESTINATION "libs/std/${ZEP_STD_VERSION}"
    COMPONENT StandardLibrary
)
