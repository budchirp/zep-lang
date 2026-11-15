import std.io.panic.panic
import std.memory.Memory

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
            return !self->is_ok()
        }

        fn unwrap() -> T {
            return self->expect("called Result.unwrap on an Error value")
        }

        fn unwrap() move -> T {
            return Memory::move(*self).expect("called Result.unwrap on an Error value")
        }

        fn expect(message: cstr) -> T {
            return when (*self) {
                Result::Ok { value } -> value,
                else -> panic(message),
            }
        }

        fn expect(message: cstr) move -> T {
            return when (*self) {
                Result::Ok { value } -> Memory::move(value),
                else -> panic(message),
            }
        }

        fn unwrap_error() -> E {
            return when (*self) {
                Result::Error { error } -> error,
                else -> panic("called Result.unwrap_error on an Ok value"),
            }
        }

        fn unwrap_error() move -> E {
            return when (*self) {
                Result::Error { error } -> Memory::move(error),
                else -> panic("called Result.unwrap_error on an Ok value"),
            }
        }

        fn unwrap_or(fallback: T) -> T {
            return when (*self) {
                Result::Ok { value } -> value,
                else -> Memory::move(fallback),
            }
        }

        fn unwrap_or(fallback: T) move -> T {
            return when (*self) {
                Result::Ok { value } -> Memory::move(value),
                else -> Memory::move(fallback),
            }
        }

        fn map<U>(mapping: (T) -> U) -> Result<U, E> {
            return when (*self) {
                Result::Ok { value } -> Result::Ok { value: mapping(value) },
                Result::Error { error } -> Result::Error { error: error },
            }
        }

        fn map<U>(mapping: (T) -> U) move -> Result<U, E> {
            return when (*self) {
                Result::Ok { value } -> Result::Ok { value: mapping(Memory::move(value)) },
                Result::Error { error } -> Result::Error { error: Memory::move(error) },
            }
        }

        fn map_error<F>(mapping: (E) -> F) -> Result<T, F> {
            return when (*self) {
                Result::Ok { value } -> Result::Ok { value: value },
                Result::Error { error } -> Result::Error { error: mapping(error) },
            }
        }

        fn map_error<F>(mapping: (E) -> F) move -> Result<T, F> {
            return when (*self) {
                Result::Ok { value } -> Result::Ok { value: Memory::move(value) },
                Result::Error { error } -> Result::Error { error: mapping(Memory::move(error)) },
            }
        }

        fn and_then<U>(mapping: (T) -> Result<U, E>) -> Result<U, E> {
            return when (*self) {
                Result::Ok { value } -> mapping(value),
                Result::Error { error } -> Result::Error { error: error },
            }
        }

        fn and_then<U>(mapping: (T) -> Result<U, E>) move -> Result<U, E> {
            return when (*self) {
                Result::Ok { value } -> mapping(Memory::move(value)),
                Result::Error { error } -> Result::Error { error: Memory::move(error) },
            }
        }

        fn or_else(mapping: (E) -> Result<T, E>) -> Result<T, E> {
            return when (*self) {
                Result::Ok { value } -> Result::Ok { value: value },
                Result::Error { error } -> mapping(error),
            }
        }

        fn or_else(mapping: (E) -> Result<T, E>) move -> Result<T, E> {
            return when (*self) {
                Result::Ok { value } -> Result::Ok { value: Memory::move(value) },
                Result::Error { error } -> mapping(Memory::move(error)),
            }
        }
}
