import std.math.Math
import std.math.MathError
import std.math.MathFailure
import std.testing.Test

test "math preserves integer and floating point overloads" {
    Test::expect(Math::absolute(-4) == 4 && Math::absolute(-4 as i64) == 4 &&
        Math::minimum(4 as i64, 7 as i64) == 4 && Math::maximum(0.5, 0.25) == 0.5 &&
        Math::clamp(2.0 as f32, 0.0 as f32, 1.0 as f32) == 1.0 as f32)
}

test "checked division reports zero divisors and signed overflow" {
    Test::expect((Math::divide(10, 2) catch { error: MathError -> 0, }) == 5)
    Test::expect((Math::divide(18446744073709551615 as u64, 1 as u64) catch {
        error: MathError -> 0 as u64,
    }) == 18446744073709551615 as u64)

    try {
        Math::divide(0, 0)
        Test::fail("division by zero succeeded")
    } catch { error: MathError -> { Test::expect(error.reason == MathFailure::DivisionByZero) }, }

    try {
        Math::divide((-2147483647) - 1, -1)
        Test::fail("signed division overflow succeeded")
    } catch { error: MathError -> { Test::expect(error.reason == MathFailure::Overflow) }, }

    try {
        Math::divide(-128 as i8, -1 as i8)
        Test::fail("narrow division overflow succeeded")
    } catch { error: MathError -> { Test::expect(error.reason == MathFailure::Overflow) }, }
}
