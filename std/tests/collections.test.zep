import std.collections.Vector
import std.collections.Map
import std.core.Box
import std.memory.Memory
import std.text.String
import std.testing.Test

test "vectors grow clear and replace initialized owners" {
    var mut values: Vector<String> = Vector()
    values.push(String("first"))
    values.resize(8, *values.get(0).unwrap())
    values.truncate(2)

    Test::expect(values.length() == 2 && values.get(1).unwrap()->equals("first"))

    values.clear()
    values.resize(3, String("again"))

    Test::expect(values.length() == 3 && values.get(2).unwrap()->equals("again"))

    values.clear()
    values.push(String("first"))
    values.push(String("last"))
    values.insert(0, String("start")).unwrap()

    Test::expect(values.remove(1).unwrap().equals("first"))
    Test::expect(values.set(0, String("replacement")).unwrap().equals("start"))

    values.shrink_to_fit()

    Test::expect(values.length() == 2 && values.get(1).unwrap()->equals("last"))

    var mut owners: Vector<Box<i32>> = Vector()
    owners.push(Box(42))
    owners.push(Box(7))
    owners.swap(0, 1)
    var removed = owners.pop()
    var owner = Memory::move(removed).unwrap()

    Test::expect(*owner.get() == 42)
    owners.clear()
}

test "maps deep copy entries and release cleared storage" {
    var mut map: Map<i32, String> = Map({ left: &i32, right: &i32 -> left == right })
    map.set(1, String("first"))
    map.set(2, String("second"))
    var copy = map
    map.clear()
    var key: i32 = 2

    Test::expect(copy.length() == 2 && copy.get(key).unwrap()->equals("second"))
}
