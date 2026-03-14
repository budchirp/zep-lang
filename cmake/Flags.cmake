function(zep_set_flags TARGET)
    target_compile_options(${TARGET} PRIVATE
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
        $<$<CONFIG:Debug>:-g3 -ggdb -O0 -fno-omit-frame-pointer -DZEP_DEBUG>
        $<$<CONFIG:Release>:-O3 -DNDEBUG -flto -march=native -ffunction-sections -fdata-sections>
    )

    target_link_options(${TARGET} PRIVATE
        $<$<CONFIG:Release>:-flto -s -Wl,--gc-sections>
    )
endfunction()