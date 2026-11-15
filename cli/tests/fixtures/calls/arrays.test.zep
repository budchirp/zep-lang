import std.memory.Memory
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
