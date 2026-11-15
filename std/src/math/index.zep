public struct Math {
    public:
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
