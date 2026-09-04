import std.core.panic.CorePanic

public enum Option<T> {
    None
    Some { value: T }

    public:
        fn is_some() -> boolean {
            return when (*self) {
                Option::Some { value } -> true,
                else -> false,
            }
        }

        fn is_none() -> boolean {
            return when (*self) {
                Option::None -> true,
                else -> false,
            }
        }

        fn unwrap() -> T {
            return when (*self) {
                Option::Some { value } -> value,
                else -> {
                    CorePanic::panic("called Option.unwrap on a None value")
                },
            }
        }

        fn expect(message: cstr) -> T {
            return when (*self) {
                Option::Some { value } -> value,
                else -> {
                    CorePanic::panic(message)
                },
            }
        }

        fn unwrap_or(default_value: T) -> T {
            return when (*self) {
                Option::Some { value } -> value,
                else -> default_value,
            }
        }

        fn unwrap_or_else(default_function: () -> T) -> T {
            return when (*self) {
                Option::Some { value } -> value,
                else -> default_function(),
            }
        }

        fn map<U>(mapping_function: (T) -> U) -> Option<U> {
            return when (*self) {
                Option::Some { value } -> Option<U>::Some {
                    value: mapping_function(value)
                },
                else -> Option<U>::None,
            }
        }

        fn and_then<U>(mapping_function: (T) -> Option<U>) -> Option<U> {
            return when (*self) {
                Option::Some { value } -> mapping_function(value),
                else -> Option<U>::None,
            }
        }

        fn filter(predicate: (*T) -> boolean) -> Option<T> {
            return when (*self) {
                Option::Some { value } -> {
                    if (predicate(&value)) {
                        Option<T>::Some { value: value }
                    } else {
                        Option<T>::None
                    }
                },
                else -> Option<T>::None,
            }
        }

        fn or_else(default_function: () -> Option<T>) -> Option<T> {
            return when (*self) {
                Option::Some { value } -> Option<T>::Some { value: value },
                else -> default_function(),
            }
        }
}
