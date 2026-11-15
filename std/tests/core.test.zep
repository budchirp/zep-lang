import std.core.Box
import std.core.Pair
import std.memory.Memory
import std.text.String
import std.testing.Test

test "boxes adopt extract and compose owned values" {
    var owner = Box(String("owned"))
    var raw = Memory::move(owner).into_raw()
    var adopted = Box::from_raw(raw)

    Test::expect(adopted.get()->equals("owned"))

    var pair = Pair(Box(1), Box(2))

    Test::expect(*pair.first.get() == 1 && *pair.second.get() == 2)

    var boxed = Box(String("boxed"))

    Test::expect(Memory::move(boxed).into_inner().equals("boxed"))
}
