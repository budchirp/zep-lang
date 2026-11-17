import std.core.Option
import std.memory.Memory
import std.text.String
import std.testing.Test

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
