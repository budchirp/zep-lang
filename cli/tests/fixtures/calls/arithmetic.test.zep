import std.math.Math
import std.testing.Test

fn add<T: i32>(left: T, right: T) -> T {
    return left + right
}

fn twice<T: i32>(value: T) -> T {
    return add(value, value)
}

fn width<T: i32>(value: T) -> i32 {
    return when (T is i64) { true -> 64, false -> 32, }
}

fn below<T: i32>(left: T, right: T) -> boolean {
    return left < right
}

fn product<T: f64>(left: T, right: T) -> T {
    return left * right
}

fn selected(early: boolean) -> i64 {
    var value = if (early) { return 99 } else { 4294967296 as i64 }

    return value
}

test "generic arithmetic preserves concrete numeric types" {
    var bounded = Math::clamp(0.5 as f32, -1.0, 1.0)
    var wide = Math::maximum(1, 4294967296 as i64)

    Test::expect(bounded == 0.5 as f32 && wide == 4294967296 as i64 &&
        Math::minimum(18446744073709551615 as u64, 0) == 0 as u64)
    Test::expect(selected(true) == 99 as i64 && selected(false) == 4294967296 as i64)
    Test::expect(add(4294967296 as i64, 7 as i64) == 4294967303 as i64)
    Test::expect(twice(4294967296 as i64) == 8589934592 as i64 && width(1 as i64) == 64)
    Test::expect(!below(18446744073709551615 as u64, 1 as u64) &&
        add(-9 as i64, -3 as i64) == -12 as i64 && add(12 as i8, 13 as i8) == 25 as i8 &&
        product(0.5 as f32, 0.25 as f32) == 0.125 as f32)
}
