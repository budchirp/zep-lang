function(zep_set_flags TARGET)
    if(ZEP_DEBUG_FULL_SYMBOLS)
        set(zep_debug_symbols -g3 -ggdb)
    else()
        set(zep_debug_symbols -gline-tables-only)
    endif()

    target_compile_options(${TARGET} PRIVATE
        -pipe
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wno-shadow
        -Wdouble-promotion
        -Wcast-qual
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Woverloaded-virtual
        -Wnull-dereference
        -Wformat=2
        -Wimplicit-fallthrough
        -Wunused
        -Wvla
        $<$<CONFIG:Debug>:${zep_debug_symbols} -O0 -fno-omit-frame-pointer -DZEP_DEBUG>
        $<$<CONFIG:Release>:-ffunction-sections -fdata-sections>
    )

    target_link_options(${TARGET} PRIVATE
        $<$<CONFIG:Release>:-Wl,--gc-sections>
    )

    if(ZEP_NATIVE)
        target_compile_options(${TARGET} PRIVATE $<$<CONFIG:Release>:-march=native>)
    endif()

    if(ZEP_ENABLE_IPO)
        set_property(TARGET ${TARGET} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    endif()
endfunction()
