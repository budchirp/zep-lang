public struct Pair<First, Second> {
    public:
        var first: First
        var second: Second

        fn Pair(first: First, second: Second) -> Pair<First, Second> {
            return Pair<First, Second> { first: first, second: second }
        }
}
