function(zep_set_sanitizers TARGET)
    set(SANITIZERS)

    if(ZEP_ENABLE_ASAN)
        list(APPEND SANITIZERS address)
    endif()

    if(ZEP_ENABLE_UBSAN)
        list(APPEND SANITIZERS undefined)
    endif()

    if(ZEP_ENABLE_TSAN)
        list(APPEND SANITIZERS thread)
    endif()

    if(ZEP_ENABLE_MSAN)
        list(APPEND SANITIZERS memory)
    endif()

    if(SANITIZERS)
        list(JOIN SANITIZERS "," SANITIZERS)
        target_compile_options(${TARGET} PRIVATE -fsanitize=${SANITIZERS} -fno-omit-frame-pointer)
        target_link_options(${TARGET} PRIVATE -fsanitize=${SANITIZERS})
    endif()
endfunction()
