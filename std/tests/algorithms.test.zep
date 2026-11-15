import std.algorithms.Algorithms
import std.algorithms.Ordering
import std.collections.Vector
import std.testing.Test

fn compare(left: &i32, right: &i32) -> Ordering {
    return when {
        (left < right) -> Ordering::Less,
        (left > right) -> Ordering::Greater,
        else -> Ordering::Equal,
    }
}

test "algorithms sort search and reverse collections" {
    var mut numbers: Vector<i32> = Vector()
    numbers.push(3)
    numbers.push(1)
    numbers.push(2)
    Algorithms::sort(numbers, compare)

    Test::expect(Algorithms::binary_search(numbers, 2, compare).unwrap() == 1 &&
        Algorithms::binary_search(numbers, 0, compare).is_none() &&
        Algorithms::binary_search(numbers, 4, compare).is_none() &&
        Algorithms::find(numbers, 3, { left: &i32, right: &i32 -> left == right }).unwrap() == 2)

    Algorithms::reverse(numbers)

    Test::expect(*numbers.first().unwrap() == 3)
}
