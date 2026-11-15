import std.memory.Memory
import std.testing.Test

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

fn reconstruct() -> void {
    var mut owner = Owner { value: 15 }
    owner.~Owner()
    new (&mut owner) Owner { value: 16 }
}

test "placement construction and explicit destruction evaluate places once" {
    var pair = Memory::allocate<Pair>()
    var placed = new (target(pair)) Pair { first: Owner { value: 1 }, second: Owner { value: 2 } }

    Test::expect(addresses == 1 && placed == pair)

    target(pair)->~Pair()

    Test::expect(addresses == 2 && destroyed == 2)

    Memory::free(pair as *mut void)

    var mut local = Owner { value: 3 }
    local.~Owner()
    local = Owner { value: 4 }
    local.~Owner()

    Test::expect(destroyed == 4)

    var choice = Memory::allocate<Choice>()
    new (choice) Choice::Some { value: Owner { value: 5 } }
    choice->~Choice()
    Memory::free(choice as *mut void)

    Test::expect(destroyed == 5)

    var source = Owner { value: 6 }
    var storage = Memory::allocate<Owner>()
    new (storage) source
    storage->~Owner()

    Test::expect(copies == 1 && destroyed == 6)

    var previous_moves = moves
    new (storage) Memory::move(source)
    storage->~Owner()
    Memory::free(storage as *mut void)

    Test::expect(source.value == 0 && moves == previous_moves + 1 && destroyed == 7)

    var allocated: *mut Pair = Memory::allocate()
    new (allocated) Pair { first: Owner { value: 7 }, second: Owner { value: 8 } }
    var selected = target(allocated)
    selected->~Pair()
    Memory::free(selected as *mut void)

    var missing: *mut Pair = null
    Test::expect(target(missing) == null)

    Test::expect(destroyed == 9 && addresses == 4)

    var array = Memory::allocate<Owners>()
    new (array) [Owner { value: 9 }, Owner { value: 10 }]
    destroy<Owners>(array)
    Memory::free(array as *mut void)

    var mut rebuilt = Pair { first: Owner { value: 11 }, second: Owner { value: 12 } }
    rebuilt.first.~Owner()
    new (&mut rebuilt.first) Owner { value: 13 }
    rebuilt.~Pair()

    var number: *mut i32 = Memory::allocate()
    new (number) 42
    destroy(number)
    Memory::free(number as *mut void)

    Test::expect(destroyed == 14)

    reconstruct()

    Test::expect(destroyed == 16)
}
