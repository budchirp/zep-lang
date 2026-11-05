import std.text.String
import std.memory.Memory
import std.collections.Vector

struct Base {
    public:
        var text: String
}

struct Derived : Base {
    public:
        var other: String
        fn Derived(source: &Derived) -> Derived = default
        fn Derived(source: move Derived) -> Derived = default
        fn operator=(source: &Derived) mut -> &mut Derived = default
        fn operator=(source: move Derived) mut -> &mut Derived = default
}

enum Payload {
    Empty
    Owned { text: String }
}

fn return_nested(values: Vector<Vector<String>>) -> Vector<Vector<String>> {
    return Memory::move(values)
}

public fn main() -> i32 {
    var mut original = Derived { text: String("base"), other: String("other") }
    var copy = original
    original.text = String("changed")
    if (!copy.text.equals("base")) { return 1 }
    original = copy
    original = original
    if (!original.text.equals("base") || !original.other.equals("other")) { return 2 }
    var moved = Memory::move(original)
    original = Memory::move(original)
    if (!moved.text.equals("base")) { return 3 }
    var mut array: String[2] = [String("first"), String("second")]
    var array_copy = array
    array[0] = String("changed")
    if (!array_copy[0].equals("first")) { return 4 }
    var mut payload = Payload::Owned { text: String("payload") }
    var payload_copy = payload
    payload = Payload::Empty
    var valid = when (payload_copy) {
        Payload::Owned { text } -> text.equals("payload"),
        else -> false,
    }
    if (!valid) { return 5 }
    var mut inner = Vector<String>()
    inner.push(String("nested"))
    var mut outer = Vector<Vector<String>>()
    outer.push(Memory::move(inner))
    var nested = return_nested(Memory::move(outer))
    if (!nested.get(0).unwrap()->get(0).unwrap()->equals("nested")) { return 6 }
    return 0
}
