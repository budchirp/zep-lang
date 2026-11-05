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

public fn main() -> i32 {
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
