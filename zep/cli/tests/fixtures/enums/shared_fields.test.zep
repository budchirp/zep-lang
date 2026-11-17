import std.text.String
import std.memory.Memory
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
