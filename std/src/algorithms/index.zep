import std.core.option.Option
import std.collections.vector.Vector

public enum Ordering {
    Less
    Equal
    Greater
}

public struct Algorithms {
    public:
        static fn reverse<T>(values: &mut Vector<T>) -> void {
            var mut left_index: i64 = 0
            var mut right_index = values.length() - 1

            while (left_index < right_index) {
                values.swap(left_index, right_index)
                left_index = left_index + 1
                right_index = right_index - 1
            }
        }

        static fn find<T>(values: &Vector<T>, needle: &T,
                          equality_function: (&T, &T) -> boolean) -> Option<i64> {
            for (var mut index: i64 = 0; index < values.length(); index = index + 1) {
                if (equality_function(*values.get(index).unwrap(), needle)) {
                    return Option::Some { value: index }
                }
            }

            return Option::None
        }

        static fn sort<T>(values: &mut Vector<T>, comparator: (&T, &T) -> Ordering) -> void {
            for (var mut index: i64 = 1; index < values.length(); index = index + 1) {
                var mut current_index = index

                while (current_index > 0) {
                    when (comparator(*values.get(current_index - 1).unwrap(),
                                     *values.get(current_index).unwrap())) {
                        Ordering::Greater -> {
                            values.swap(current_index - 1, current_index)
                            current_index = current_index - 1
                        },
                        else -> current_index = 0,
                    }
                }
            }
        }

        static fn binary_search<T>(values: &Vector<T>, needle: &T,
                                   comparator: (&T, &T) -> Ordering) -> Option<i64> {
            var mut lower_bound: i64 = 0
            var mut upper_bound = values.length()

            while (lower_bound < upper_bound) {
                var middle_index = lower_bound + ((upper_bound - lower_bound) / 2)
                when (comparator(*values.get(middle_index).unwrap(), needle)) {
                    Ordering::Equal -> { return Option::Some { value: middle_index } },
                    Ordering::Less -> lower_bound = middle_index + 1,
                    Ordering::Greater -> upper_bound = middle_index,
                }
            }

            return Option::None
        }
}
