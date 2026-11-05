@os("linux")
@arch("x86_64")
public fn main() -> i32 {
    var mut total: i64 = 20
    asm {
        "addq {input}, {total}"

        in(input, "r", 22 as i64)
        inout(total, "r", total)

        clobber("flags")
    }

    var mut fixed: i64 = 0
    asm {
        "movq {input}, {output}"
        in(input, "r", total)
        out(output, "rax", fixed)
        options("pure", "nomem")
    }

    var mut cycles_low: u32 = 0
    var mut cycles_high: u32 = 0
    asm {
        "rdtsc"
        out(low, "eax", cycles_low)
        out(high, "edx", cycles_high)
        options("nomem")
    }

    var mut process: i64 = 39
    asm {
        "syscall"
        inout(number, "rax", process)
        clobber("rcx", "r11", "flags")
    }

    return if (fixed == 42 && process > 0) { 0 } else { 1 }
}
