import std.memory.Memory
import std.text.string.String

extern fn putchar(value: i32) -> i32

struct Owner {
    public:
        var value: i32
        var memory: *mut void

        fn ~Owner() -> void {
            Memory::free(memory)
            putchar(value)
        }
}

fn make_owner(value: i32) -> Owner {
    return Owner { value: value, memory: Memory::allocate_bytes(16) }
}

fn consume(owner: Owner) -> void {}

public fn main() -> i32 {
    make_owner(65)
    var owner = make_owner(66)
    consume(make_owner(67))
    if (true) {
        make_owner(68)
        putchar(69)
    }
    if (false) {
        make_owner(90)
        putchar(90)
    }
    String::from(42)
    owner
    return 0
}
