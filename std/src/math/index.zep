public struct Math {
    public:
        static fn absolute(value: i32) -> i32 {
            return if (value < 0) { -value } else { value }
        }

        static fn absolute(value: i64) -> i64 {
            return if (value < (0 as i64)) { -value } else { value }
        }

        static fn absolute(value: f32) -> f32 {
            return if (value < (0.0 as f32)) { -value } else { value }
        }

        static fn absolute(value: f64) -> f64 {
            return if (value < 0.0) { -value } else { value }
        }

        static fn minimum(left: i32, right: i32) -> i32 {
            return if (left < right) { left } else { right }
        }

        static fn minimum(left: i64, right: i64) -> i64 {
            return if (left < right) { left } else { right }
        }

        static fn minimum(left: f32, right: f32) -> f32 {
            return if (left < right) { left } else { right }
        }

        static fn minimum(left: f64, right: f64) -> f64 {
            return if (left < right) { left } else { right }
        }

        static fn maximum(left: i32, right: i32) -> i32 {
            return if (left > right) { left } else { right }
        }

        static fn maximum(left: i64, right: i64) -> i64 {
            return if (left > right) { left } else { right }
        }

        static fn maximum(left: f32, right: f32) -> f32 {
            return if (left > right) { left } else { right }
        }

        static fn maximum(left: f64, right: f64) -> f64 {
            return if (left > right) { left } else { right }
        }

        static fn clamp(value: i32, minimum: i32, maximum: i32) -> i32 {
            if (value < minimum) { return minimum }
            if (value > maximum) { return maximum }
            return value
        }

        static fn clamp(value: i64, minimum: i64, maximum: i64) -> i64 {
            if (value < minimum) { return minimum }
            if (value > maximum) { return maximum }
            return value
        }

        static fn clamp(value: f32, minimum: f32, maximum: f32) -> f32 {
            if (value < minimum) { return minimum }
            if (value > maximum) { return maximum }
            return value
        }

        static fn clamp(value: f64, minimum: f64, maximum: f64) -> f64 {
            if (value < minimum) { return minimum }
            if (value > maximum) { return maximum }
            return value
        }
}
