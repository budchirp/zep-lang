import std.core.option.Option
import std.collections.vector.Vector

public enum Ordering {
    Less
    Equal
    Greater
}

public type Comparator<T> = (*T, *T) -> Ordering

public struct Algorithms {
    public:
        static fn reverse<T>(values: *mut Vector<T>) -> void {
            var mut left_index: i64 = 0
            var mut right_index = values->length() - 1

            while (left_index < right_index) {
                values->swap(left_index, right_index)
                left_index = left_index + 1
                right_index = right_index - 1
            }
        }

        static fn find<T>(values: *Vector<T>, needle: *T,
                          equality_function: (*T, *T) -> boolean) -> Option<i64> {
            for (var mut index: i64 = 0; index < values->length(); index = index + 1) {
                var candidate = values->get(index)
                if (candidate.is_some() && equality_function(candidate.unwrap(), needle)) {
                    return Option<i64>::Some { value: index }
                }
            }

            return Option<i64>::None
        }

        static fn sort<T>(values: *mut Vector<T>, comparator: Comparator<T>) -> void {
            for (var mut index: i64 = 1; index < values->length(); index = index + 1) {
                var mut current_index = index

                while (current_index > 0) {
                    var left = values->get(current_index - 1).unwrap()
                    var right = values->get(current_index).unwrap()
                    if (comparator(left, right).value != Ordering::Greater.value) {
                        current_index = 0
                    } else {
                        values->swap(current_index - 1, current_index)
                        current_index = current_index - 1
                    }
                }
            }
        }

        static fn binary_search<T>(values: *Vector<T>, needle: *T,
                                   comparator: Comparator<T>) -> Option<i64> {
            var mut lower_bound: i64 = 0
            var mut upper_bound = values->length()

            while (lower_bound < upper_bound) {
                var middle_index = lower_bound + ((upper_bound - lower_bound) / 2)
                var ordering = comparator(values->get(middle_index).unwrap(), needle)

                if (ordering.value == Ordering::Equal.value) {
                    return Option<i64>::Some { value: middle_index }
                }

                if (ordering.value == Ordering::Less.value) {
                    lower_bound = middle_index + 1
                } else {
                    upper_bound = middle_index
                }
            }

            return Option<i64>::None
        }
}
