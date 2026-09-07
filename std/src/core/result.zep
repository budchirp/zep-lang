import std.io.panic.panic

public enum Result<T, E> {
    Ok { value: T }
    Error { error: E }

    public:
        fn is_ok() -> boolean {
            return when (*self) {
                Result::Ok { value } -> true,
                else -> false,
            }
        }

        fn is_error() -> boolean {
            return when (*self) {
                Result::Error { error } -> true,
                else -> false,
            }
        }

        fn unwrap() -> T {
            return when (*self) {
                Result::Ok { value } -> value,
                else -> {
                    panic("called Result.unwrap on an Error value")
                },
            }
        }

        fn expect(message: cstr) -> T {
            return when (*self) {
                Result::Ok { value } -> value,
                else -> {
                    panic(message)
                },
            }
        }

        fn unwrap_error() -> E {
            return when (*self) {
                Result::Error { error } -> error,
                else -> {
                    panic("called Result.unwrap_error on an Ok value")
                },
            }
        }

        fn unwrap_or(default_value: T) -> T {
            return when (*self) {
                Result::Ok { value } -> value,
                else -> default_value,
            }
        }

        fn map<U>(mapping_function: (T) -> U) -> Result<U, E> {
            return when (*self) {
                Result::Ok { value } -> Result<U, E>::Ok {
                    value: mapping_function(value)
                },
                Result::Error { error } -> Result<U, E>::Error {
                    error: error
                },
            }
        }

        fn map_error<F>(mapping_function: (E) -> F) -> Result<T, F> {
            return when (*self) {
                Result::Ok { value } -> Result<T, F>::Ok { value: value },
                Result::Error { error } -> Result<T, F>::Error {
                    error: mapping_function(error)
                },
            }
        }

        fn and_then<U>(mapping_function: (T) -> Result<U, E>) -> Result<U, E> {
            return when (*self) {
                Result::Ok { value } -> mapping_function(value),
                Result::Error { error } -> Result<U, E>::Error {
                    error: error
                },
            }
        }

        fn or_else(mapping_function: (E) -> Result<T, E>) -> Result<T, E> {
            return when (*self) {
                Result::Ok { value } -> Result<T, E>::Ok { value: value },
                Result::Error { error } -> mapping_function(error),
            }
        }
}
