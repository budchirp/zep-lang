import dispatch_types.Measure
import dispatch_types.Count
import dispatch_types.Derived
import std.testing.Test

var mut evaluations = 0

fn make() -> Derived {
    evaluations = evaluations + 1
    return Derived { count: 42 }
}

fn read(value: Measure) -> i64 {
    return value.measure() + value.measure(1)
}

test "interface dispatch preserves overloads inheritance and receiver types" {
    Test::expect(read(make()) == 85 as i64 && evaluations == 1)

    var wide: Count<i64> = Count { count: 4294967296 as i64 }

    Test::expect(read(wide) == 8589934593 as i64)

    var count: Count<i32> = Count { count: -7 }

    Test::expect(read(count) == -13 as i64)
}
