import std.memory.Memory

public struct Pair<First, Second> {
    public:
        var first: First
        var second: Second

        fn Pair(first: First, second: Second) -> Pair<First, Second> {
            return Pair { first: Memory::move(first), second: Memory::move(second) }
        }
}
