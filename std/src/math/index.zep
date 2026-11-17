import std.core.error.Error

public enum MathFailure : i32 {
    DivisionByZero = 0
    Overflow = 1
}

public struct MathError : Error {
    public:
        var reason: MathFailure
}

public struct Math {
    public:
        @throws(MathError)
        static fn divide<T: i32>(left: T, right: T) -> T {
            if (right == 0) {
                throw MathError { reason: MathFailure::DivisionByZero }
            }

            if (right < 0 && right + 1 == 0 && left == Integer::minimum_value()) {
                throw MathError { reason: MathFailure::Overflow }
            }

            return left / right
        }

        static fn absolute(value: i32) -> i32 {
            return when { (value < 0) -> -value, else -> value, }
        }

        static fn absolute(value: i64) -> i64 {
            return when { (value < 0) -> -value, else -> value, }
        }

        static fn absolute<T: f64>(value: T) -> T {
            return when { (value < 0.0) -> -value, else -> value, }
        }

        static fn minimum<T: i32>(left: T, right: T) -> T {
            return when { (left < right) -> left, else -> right, }
        }

        static fn minimum<T: f64>(left: T, right: T) -> T {
            return when { (left < right) -> left, else -> right, }
        }

        static fn maximum<T: i32>(left: T, right: T) -> T {
            return when { (left > right) -> left, else -> right, }
        }

        static fn maximum<T: f64>(left: T, right: T) -> T {
            return when { (left > right) -> left, else -> right, }
        }

        static fn clamp<T: i32>(value: T, minimum: T, maximum: T) -> T {
            return Math::minimum(Math::maximum(value, minimum), maximum)
        }

        static fn clamp<T: f64>(value: T, minimum: T, maximum: T) -> T {
            return Math::minimum(Math::maximum(value, minimum), maximum)
        }
}
