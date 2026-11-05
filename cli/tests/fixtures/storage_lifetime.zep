import std.memory.Memory

var mut destroyed: i32 = 0
var mut copies: i32 = 0
var mut moves: i32 = 0
var mut addresses: i32 = 0

struct Owner {
    public:
        var value: i32

        fn Owner(source: &Owner) -> Owner {
            copies = copies + 1
            return Owner { value: source.value }
        }

        fn Owner(source: move Owner) -> Owner {
            moves = moves + 1
            var value = source.value
            source.value = 0
            return Owner { value: value }
        }

        fn ~Owner() -> void {
            if (value != 0) {
                destroyed = destroyed + 1
            }
        }
}

struct Pair {
    public:
        var first: Owner
        var second: Owner
}

enum Choice {
    Some { value: Owner }
    Other { first: Owner, second: Owner }
}

type Owners = Owner[2]

fn destroy<T>(ptr: *mut T) -> void {
    ptr->~T()
}

fn target(ptr: *mut Pair) -> *mut Pair {
    addresses = addresses + 1
    return ptr
}

public fn main() -> i32 {
    var pair = Memory::allocate<Pair>()
    var placed = new (target(pair)) Pair { first: Owner { value: 1 }, second: Owner { value: 2 } }

    if (addresses != 1 || placed != pair) { return 1 }

    target(pair)->~Pair()

    if (addresses != 2 || destroyed != 2) { return 2 }

    Memory::free(pair as *mut void)

    var mut local = Owner { value: 3 }
    local.~Owner()
    local = Owner { value: 4 }
    local.~Owner()

    if (destroyed != 4) { return 3 }

    var choice = Memory::allocate<Choice>()
    new (choice) Choice::Some { value: Owner { value: 5 } }
    delete choice

    if (destroyed != 5) { return 4 }

    var source = Owner { value: 6 }
    var storage = Memory::allocate<Owner>()
    new (storage) source
    storage->~Owner()

    if (copies != 1 || destroyed != 6) { return 5 }

    var previous_moves = moves
    new (storage) Memory::move(source)
    delete storage

    if (source.value != 0 || moves != previous_moves + 1 || destroyed != 7) { return 6 }

    var allocated = new Pair { first: Owner { value: 7 }, second: Owner { value: 8 } }
    delete target(allocated)
    delete null
    var missing: *mut Pair = null
    delete target(missing)

    if (destroyed != 9 || addresses != 4) { return 7 }

    var array = Memory::allocate<Owners>()
    new (array) [Owner { value: 9 }, Owner { value: 10 }]
    array->~Owners()
    Memory::free(array as *mut void)

    var mut rebuilt = Pair { first: Owner { value: 11 }, second: Owner { value: 12 } }
    rebuilt.first.~Owner()
    new (&mut rebuilt.first) Owner { value: 13 }
    rebuilt.~Pair()

    var number = new 42
    destroy(number)
    Memory::free(number as *mut void)

    if (destroyed != 14) { return 8 }

    return 0
}
