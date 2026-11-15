import std.memory.Memory
import std.text.String

extern fn putchar(value: i32) -> i32

struct Counter {
    public:
        var value: i32
        fn ~Counter() -> void { putchar(value) }
}

struct Nested {
    public:
        var counter: Counter
}

enum Choice {
    Empty
    Owned { counter: Counter }
}

var global = Counter { value: 70 }

fn take(value: Nested) -> void {}
fn make() -> Nested { return Nested { counter: Counter { value: 65 } } }

fn order() -> i32 {
    var outer = Nested { counter: Counter { value: 66 } }
    do {
        var copied = outer
        take(copied)
        0
    }
    do {
        var array: Counter[2] = [Counter { value: 67 }, Counter { value: 68 }]
        0
    }
    var mut choice = Choice::Owned { counter: Counter { value: 69 } }
    choice = Choice::Empty
    make()
    var mut rebuilt = Counter { value: 71 }
    rebuilt.~Counter()
    rebuilt = Counter { value: 72 }
    return 0
}

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

fn discarded() -> void {
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
}

public fn main() -> i32 {
    discarded()

    return order()
}
