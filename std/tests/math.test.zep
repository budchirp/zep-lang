import std.math.Math
import std.testing.Test

test "math preserves integer and floating point overloads" {
    Test::expect(Math::absolute(-4) == 4 && Math::absolute(-4 as i64) == 4 &&
        Math::minimum(4 as i64, 7 as i64) == 4 && Math::maximum(0.5, 0.25) == 0.5 &&
        Math::clamp(2.0 as f32, 0.0 as f32, 1.0 as f32) == 1.0 as f32)
}
