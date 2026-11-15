import std.core.Result
import std.memory.Memory
import std.testing.Test

var mut calls = 0

fn tick() -> void {
    calls = calls + 1
}

fn success() -> Result<void, i32> {
    return Result::Ok
}

fn number() -> i64 {
    return 42
}

fn failed() -> Result<void, i32> {
    return Result::Error { error: 7 }
}

fn forward() -> Result<void, i32> {
    calls = calls + 1

    return Result::Ok
}

fn widen(value: i32) -> i64 {
    return value as i64
}

test "void results erase storage and adapt callbacks" {
    success().unwrap()
    var value: Result<void, i32> = Result::Ok { value: tick() }
    var mapped = Memory::move(value).map(number)

    Test::expect(calls == 1 && mapped.unwrap() == 42 as i64)

    var failure: Result<void, i32> = Result::Error { error: 7 }

    Test::expect(failure.unwrap_error() == 7)

    success().map(tick).unwrap()
    success().and_then(forward).expect("forward failed")

    Test::expect(failed().map_error(widen).unwrap_error() == 7 as i64 &&
        failed().map(tick).unwrap_error() == 7 &&
        failed().and_then(forward).unwrap_error() == 7 && calls == 3)

    var callback = number

    Test::expect(success().map(callback).unwrap() == 42 as i64)
}
