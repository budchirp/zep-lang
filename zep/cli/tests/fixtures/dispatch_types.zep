public interface Measure {
    public:
        fn measure() -> i64
        fn measure(offset: i32) -> i64
}

public struct Count<T: i32> : Measure {
    public:
        var count: T

        override fn measure() -> i64 {
            return count as i64
        }

        override fn measure(offset: i32) -> i64 {
            return (count as i64) + (offset as i64)
        }
}

public struct Derived : Count<i64> {}
