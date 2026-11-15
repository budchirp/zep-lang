import std.text.String
import std.memory.Memory
import std.core.Result
import std.core.Option
import std.testing.Test

enum Event<T>(context: T) {
    Ready
    Data { value: i64 }
}

var mut order = 0

struct Marker {
    public:
        var value: i32

        fn ~Marker() -> void {
            order = order * 10 + self->value
        }
}

enum Tracked(context: Marker) {
    Ready
    Data { value: Marker }
}

fn cleanup() -> void {
    var value = Tracked::Data { context: Marker { value: 1 }, value: Marker { value: 2 } }
    order = 0
}

test "shared fields copy move and destroy after the active payload" {
    cleanup()

    Test::expect(order == 21)

    var mut event: Event<String> = Event::Data { context: String("source"), value: 42 }
    var copied = event
    event.context = String("changed")

    Test::expect(copied.context.equals("source"))

    var result = when (event) {
        Event::Ready -> 0 as i64,
        Event::Data { context, value } -> {
            context.append("!")
            value
        },
    }

    Test::expect(result == 42 as i64 && event.context.equals("changed!"))

    var moved = Memory::move(event)

    Test::expect(event.context.is_empty() && moved.context.equals("changed!"))
}

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

enum Small {
    Item { value: i32 }
}

test "payload bindings borrow original storage and extend temporaries" {
    var mut source: Option<String> = Option::Some { value: String("owned") }
    var moved = when (source) {
        Option::Some { value } -> Memory::move(value),
        Option::None -> String(),
    }

    Test::expect(moved.equals("owned"))

    var temporary = when (Option::Some { value: String("temporary") }) {
        Option::Some { value } -> Memory::move(value),
        Option::None -> String(),
    }

    Test::expect(temporary.equals("temporary"))

    var guarded = when (Small::Item { value: 1 }) {
        Small::Item { value } if ((value = 2) == 0) -> 0,
        Small::Item { value } -> value,
    }

    Test::expect(guarded == 2)

    Test::expect(when (source) {
        Option::Some { value } -> value.is_empty(),
        Option::None -> false,
    })
}
