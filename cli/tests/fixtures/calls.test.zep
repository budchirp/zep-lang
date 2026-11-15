import std.memory.Memory
import std.math.Math
import std.testing.Test

type Row = i32[3]
type Alias = Row
type Grid = Row[2]

fn identity<T>(ptr: *mut T) -> *mut T {
    return ptr
}

fn last(ptr: *Alias) -> i32 {
    return ptr[0][2]
}

fn read_grid(ptr: *Grid) -> i32 {
    return ptr[0][1][2]
}

fn size<T>(value: &T) -> i64 {
    return #sizeof(T)
}

fn choose(ptr: *Row) -> i32 { return 1 }
fn choose(ptr: *mut Row) -> i32 { return 4 }
fn choose(ptr: *i32) -> i32 { return 2 }
fn choose(ptr: *void) -> i32 { return 3 }

test "array aliases preserve shape and bind across call forms" {
    var rows = Memory::allocate_many<Row>(2)
    new (&mut rows[0]) [1, 2, 3]
    new (&mut rows[1]) [4, 5, 6]

    var inferred = identity(rows)
    var explicit = identity<Alias>(rows)
    var named = identity(ptr: rows)
    var callback = last

    Test::expect(inferred == rows && explicit == rows && named == rows)
    Test::expect(last(ptr: &rows[1]) == 6 && callback(rows) == 3)
    Test::expect(size(rows[0]) == 12)
    var view: *Row = rows

    Test::expect(choose(rows) == 4 && choose(view) == 1)

    explicit[1][2] = 42

    Test::expect(rows[0][2] == 3 && rows[1][2] == 42)

    var mut grid: Grid = [[7, 8, 9], [10, 11, 12]]
    var storage: *mut Grid = Memory::allocate()
    new (storage) grid
    var same = identity<Grid>(storage)
    var reader = read_grid

    Test::expect(same == storage && reader(same) == 12)

    same[0][1][2] = 43

    Test::expect(storage[0][0][2] == 9 && storage[0][1][2] == 43)

    storage->~Grid()
    Memory::free(storage as *mut void)
    Memory::free(rows as *mut void)

}

struct Callable<T> {
    public:
        var function: (T) -> T

        fn apply(value: T) -> T {
            return function(value)
        }

        fn apply_with(function: (T) -> T, value: T) -> T {
            return function(value)
        }
}

var mut evaluations = 0

fn increment(value: i64) -> i64 {
    return value + 1
}

fn decrement(value: i64) -> i64 {
    return value - 1
}

fn make() -> Callable<i64> {
    evaluations = evaluations + 1

    return Callable { function: increment }
}

fn retain<T>(value: T) -> T {
    return value
}

fn invert(value: boolean) -> boolean {
    return !value
}

test "callable fields preserve signatures and evaluate receivers once" {
    var callable = make()
    var first = retain(increment)
    var second = retain(decrement)
    var third = retain(invert)

    Test::expect(first(41) == 42 as i64 && second(43) == 42 as i64 && third(false))

    Test::expect(callable.apply(41) == 42 as i64 && callable.function(41) == 42 as i64 &&
        callable.apply_with(decrement, 43) == 42 as i64 &&
        make().function(41) == 42 as i64 && evaluations == 2)
}

fn add<T: i32>(left: T, right: T) -> T {
    return left + right
}

fn twice<T: i32>(value: T) -> T {
    return add(value, value)
}

fn width<T: i32>(value: T) -> i32 {
    return when (T is i64) { true -> 64, false -> 32, }
}

fn below<T: i32>(left: T, right: T) -> boolean {
    return left < right
}

fn product<T: f64>(left: T, right: T) -> T {
    return left * right
}

fn selected(early: boolean) -> i64 {
    var value = if (early) { return 99 } else { 4294967296 as i64 }

    return value
}

test "generic arithmetic preserves concrete numeric types" {
    var bounded = Math::clamp(0.5 as f32, -1.0, 1.0)
    var wide = Math::maximum(1, 4294967296 as i64)

    Test::expect(bounded == 0.5 as f32 && wide == 4294967296 as i64 &&
        Math::minimum(18446744073709551615 as u64, 0) == 0 as u64)

    Test::expect(selected(true) == 99 as i64 && selected(false) == 4294967296 as i64)

    Test::expect(add(4294967296 as i64, 7 as i64) == 4294967303 as i64)

    Test::expect(twice(4294967296 as i64) == 8589934592 as i64 && width(1 as i64) == 64)

    Test::expect(!below(18446744073709551615 as u64, 1 as u64) &&
        add(-9 as i64, -3 as i64) == -12 as i64 && add(12 as i8, 13 as i8) == 25 as i8 &&
        product(0.5 as f32, 0.25 as f32) == 0.125 as f32)
}
