import std.memory.Memory
import std.testing.Test

var mut calls = 0

enum Outcome<T> {
    Value { value: T }
    Empty
}

fn tick() -> void {
    calls = calls + 1
}

fn number() -> i64 {
    return 42
}

fn make<T>(value: T) -> Outcome<T> {
    return Outcome::Value { value: value }
}

fn apply<T, U>(source: Outcome<T>, operation: (T) -> U) -> Outcome<U> {
    return when (source) {
        Outcome::Value { value } -> Outcome::Value { value: operation(value) },
        Outcome::Empty -> Outcome::Empty,
    }
}

test "void payloads erase storage and adapt callbacks" {
    var value: Outcome<void> = Outcome::Value { value: tick() }
    var mapped = apply(Memory::move(value), number)

    Test::expect(calls == 1 && #sizeof(Outcome<void>) == 4 && when (mapped) {
        Outcome::Value { value } -> value == 42 as i64,
        Outcome::Empty -> false,
    })

    var forwarded = make(tick())
    var folded = apply(forwarded, tick)

    Test::expect(calls == 3 && when (folded) {
        Outcome::Value -> true,
        Outcome::Empty -> false,
    })

    var callback = number
    var indirect = apply(make(tick()), callback)

    Test::expect(calls == 4 && when (indirect) {
        Outcome::Value { value } -> value == 42 as i64,
        Outcome::Empty -> false,
    })

    var empty: Outcome<void> = Outcome::Empty
    var skipped = apply(empty, tick)

    Test::expect(calls == 4 && when (skipped) {
        Outcome::Value -> false,
        Outcome::Empty -> true,
    })
}
