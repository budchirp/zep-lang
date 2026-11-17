import std.testing.Test
import std.core.error.Throwable
import std.core.error.Error
import std.core.error.Exception
import std.memory.Memory
import std.core.Box
import std.text.String

struct ReadError : Error {}
struct ParseError : Exception {}

struct DirectError : Throwable { public: var code: i32 }

test "throwable structs carry their own payload" {
    var code = try {
        throw DirectError { code: 42 }
    } catch { error: DirectError -> error.code, }

    Test::expect(code == 42)
}

@throws(ReadError, ParseError)
fn read(fail: i32) -> i64 {
    when (fail) {
        1 -> throw ReadError {},
        2 -> throw ParseError {},
        else -> {},
    }

    return 4294967296 as i64
}

@throws(ParseError)
fn forward(fail: i32) -> i64 {
    return try read(fail) catch { error: ReadError -> 7 as i64, }
}

@throws(ReadError)
fn stop() -> never {
    throw ReadError {}
}

@throws(ReadError)
fn nothing(fail: boolean) -> void {
    if (fail) {
        throw ReadError {}
    }
}

@throws(E)
fn raise<E: Throwable>(value: E) -> never {
    throw Memory::move(value)
}

interface Reader {
    public:
        @throws(ReadError)
        fn read() -> i32
}

struct Source : Reader {
    public:
        @throws(ReadError)
        override fn read() -> i32 {
            throw ReadError {}
        }
}

@throws(ReadError)
fn read_interface(value: Reader) -> i32 {
    return try value.read()
}

test "generic errors contextual callbacks and interfaces retain their error sets" {
    var mut code = 0

    raise(DirectError { code: 3 }) catch { error: DirectError -> { code = error.code; do {} }, }

    Test::expect(code == 3)
    raise(ReadError {}) catch { error: ReadError -> {}, }

    var callback: @throws(ReadError) () -> i32 = { -> throw ReadError {} }

    Test::expect((callback() catch { error: ReadError -> 4, }) == 4)

    var source = Source {}

    Test::expect((read_interface(source) catch { error: ReadError -> 5, }) == 5)
}

test "success and typed handlers preserve values" {
    Test::expect((read(0) catch {
        error: ReadError -> 1 as i64,
        error: ParseError -> 2 as i64,
    }) == 4294967296 as i64)

    Test::expect((read(1) catch {
        error: ReadError -> 1 as i64,
        error: ParseError -> 2 as i64,
    }) == 1 as i64)

    Test::expect((forward(2) catch { error: ParseError -> 2 as i64, }) == 2 as i64)
    Test::expect((forward(1) catch { error: ParseError -> 2 as i64, }) == 7 as i64)
}

test "handler failures reach outer handlers" {
    var value = try {
        read(1) catch { error: ReadError -> try forward(2), }
    } catch { error: ParseError -> 42 as i64, }

    Test::expect(value == 42 as i64)
}

test "void never and indirect calls use the same error convention" {
    var callback: @throws(ParseError, ReadError) (i32) -> i64 = read

    Test::expect((callback(2) catch {
        error: ReadError -> 1 as i64,
        error: ParseError -> 2 as i64,
    }) == 2 as i64)

    nothing(false) catch { error: ReadError -> {}, }
    nothing(true) catch { error: ReadError -> {}, }
    stop() catch { error: ReadError -> {}, }
}

var mut trace = 0

struct Owner {
    public:
        var value: i32

        fn ~Owner() -> void {
            trace = trace * 10 + value
        }
}

struct Pair {
    public:
        var first: Owner
        var second: Owner
}

struct OwnedError : Error { public: var owner: Owner }
struct TextError : Exception { public: var message: String }

struct MoveError : Error { public: var owner: Box<i32> }

test "move-only errors transfer their payload out of a handler" {
    var owner = raise(MoveError { owner: Box(42) }) catch {
        error: MoveError -> Memory::move(error.owner),
    }

    Test::expect(*owner.get() == 42)
}

@throws(ReadError)
fn fail_owner() -> Owner {
    throw ReadError {}
}

@throws(ReadError)
fn fail_argument(owner: Owner) -> void {
    throw ReadError {}
}

fn consume(first: Owner, second: Owner) -> void {}

@throws(OwnedError)
fn fail_owned() -> void {
    throw OwnedError { owner: Owner { value: 7 } }
}

@throws(TextError)
fn fail_text() -> void {
    var message = String("retained")
    throw TextError { message: message }
}

test "failure cleanup precedes handlers and destroys owned errors once" {
    try {
        var local = Owner { value: 1 }
        fail_argument(Owner { value: 2 })
    } catch { error: ReadError -> { Test::expect(trace == 21) }, }

    trace = 0

    fail_owned() catch { error: OwnedError -> { Test::expect(error.owner.value == 7 && trace == 0) }, }

    Test::expect(trace == 7)

    fail_text() catch { error: TextError -> { Test::expect(error.message.equals("retained")) }, }
}

test "failed arguments and partial aggregates release initialized values" {
    try {
        consume(Owner { value: 3 }, fail_owner())
    } catch { error: ReadError -> {}, }

    Test::expect(trace == 3)
    trace = 0

    try {
        var pair = Pair { first: Owner { value: 5 }, second: fail_owner() }
        do {}
    } catch { error: ReadError -> {}, }

    Test::expect(trace == 5)
}

test "failed initialization preserves destinations and raw storage" {
    var mut destination = Owner { value: 4 }

    try {
        destination = fail_owner()
        do {}
    } catch { error: ReadError -> {}, }

    Test::expect(destination.value == 4 && trace == 0)

    var storage: *mut Owner = Memory::allocate()

    try {
        new (storage) fail_owner()
        do {}
    } catch { error: ReadError -> {}, }

    Memory::free(storage as *mut void)

    Test::expect(trace == 0)
}
