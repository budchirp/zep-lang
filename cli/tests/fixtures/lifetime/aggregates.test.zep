import std.collections.Vector
import std.memory.Memory
import std.text.String
import std.testing.Test

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

test "aggregate ownership handles bases arrays payloads and self assignment" {
    var mut original = Derived { text: String("base"), other: String("other") }
    var copy = original
    original.text = String("changed")

    Test::expect(copy.text.equals("base"))

    original = copy
    original = original
    original.text = original.text

    Test::expect(original.text.equals("base") && original.other.equals("other"))

    var moved = Memory::move(original)
    original = Memory::move(original)

    Test::expect(moved.text.equals("base"))

    var mut array: String[2] = [String("first"), String("second")]
    var array_copy = array
    array[0] = String("changed")

    Test::expect(array_copy[0].equals("first"))

    var mut payload = Payload::Owned { text: String("payload") }
    var payload_copy = payload
    payload = Payload::Empty
    var valid = when (payload_copy) {
        Payload::Owned { text } -> text.equals("payload"),
        else -> false,
    }

    Test::expect(valid)

    var mut inner: Vector<String> = Vector()
    inner.push(String("nested"))
    var copied = inner
    inner.clear()

    Test::expect(copied.get(0).unwrap()->equals("nested"))

    var mut outer: Vector<Vector<String>> = Vector()
    outer.push(Memory::move(copied))
    var nested = return_nested(Memory::move(outer))

    Test::expect(nested.get(0).unwrap()->get(0).unwrap()->equals("nested"))

    var storage: *mut String = Memory::allocate()
    new (storage) String("stored")
    storage[0] = storage[0]

    Test::expect(storage[0].equals("stored"))

    storage->~String()
    Memory::free(storage as *mut void)
}
