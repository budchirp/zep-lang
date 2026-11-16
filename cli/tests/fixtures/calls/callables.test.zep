import std.testing.Test

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

test "captured locals retain their identity through shadowing" {
    var value: i64 = 40

    Test::expect({ amount: i64 -> value + amount }(2) == 42 as i64)

    do {
        var value: i64 = 7

        Test::expect({ -> value }() == 7 as i64)
        0
    }

    Test::expect({ -> value }() == 40 as i64)

    var reference: &i64 = value

    Test::expect({ -> reference }() == 40 as i64)
}
