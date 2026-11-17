import std.testing.Test

enum Direction { North South }

var mut calls = 0

fn direction() -> Direction {
    calls = calls + 1

    return Direction::North
}

test "plain enum comparisons use tags and evaluate operands once" {
    Test::expect(direction() == Direction::North)
    Test::expect(direction() != Direction::South)
    Test::expect(calls == 2)
}
