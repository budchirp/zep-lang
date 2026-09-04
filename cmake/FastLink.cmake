option(ZEP_USE_FAST_LINKER "Use mold when available (Clang only)" ON)

if(NOT ZEP_USE_FAST_LINKER)
    return()
endif()

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    return()
endif()

find_program(ZEP_MOLD_EXECUTABLE NAMES mold DOC "mold linker")
if(ZEP_MOLD_EXECUTABLE)
    add_link_options(-fuse-ld=mold)
    message(STATUS "zep: mold linker enabled")
endif()
