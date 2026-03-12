function(zep_set_compiler_warnings target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Woverloaded-virtual
        -Wnull-dereference
        -Wformat=2
        -Wimplicit-fallthrough
        -Wunused
    )

    target_compile_options(${target} PRIVATE
        $<$<CONFIG:Debug>:-g3 -ggdb -O0 -fno-omit-frame-pointer -DZEP_DEBUG>
    )

    target_compile_options(${target} PRIVATE
        $<$<CONFIG:Release>:-O3 -DNDEBUG -flto -march=native>
    )
    target_link_options(${target} PRIVATE
        $<$<CONFIG:Release>:-flto -s>
    )
endfunction()
