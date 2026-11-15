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
