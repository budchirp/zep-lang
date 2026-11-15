import std.io.panic.panic
import std.memory.Memory

public enum Option<T> {
    None
    Some { value: T }

    public:
        fn is_some() -> boolean {
            return when (*self) {
                Option::None -> false,
                else -> true,
            }
        }

        fn is_none() -> boolean {
            return !self->is_some()
        }

        fn unwrap() -> T {
            return self->expect("called Option.unwrap on a None value")
        }

        fn unwrap() move -> T {
            return Memory::move(*self).expect("called Option.unwrap on a None value")
        }

        fn expect(message: cstr) -> T {
            return when (*self) {
                Option::Some { value } -> value,
                else -> panic(message),
            }
        }

        fn expect(message: cstr) move -> T {
            return when (*self) {
                Option::Some { value } -> Memory::move(value),
                else -> panic(message),
            }
        }

        fn unwrap_or(fallback: T) -> T {
            return when (*self) {
                Option::Some { value } -> value,
                else -> Memory::move(fallback),
            }
        }

        fn unwrap_or(fallback: T) move -> T {
            return when (*self) {
                Option::Some { value } -> Memory::move(value),
                else -> Memory::move(fallback),
            }
        }

        fn unwrap_or_else(fallback: () -> T) -> T {
            return when (*self) {
                Option::Some { value } -> value,
                else -> fallback(),
            }
        }

        fn unwrap_or_else(fallback: () -> T) move -> T {
            return when (*self) {
                Option::Some { value } -> Memory::move(value),
                else -> fallback(),
            }
        }

        fn map<U>(mapping: (T) -> U) -> Option<U> {
            return when (*self) {
                Option::Some { value } -> Option::Some { value: mapping(value) },
                else -> Option::None,
            }
        }

        fn map<U>(mapping: (T) -> U) move -> Option<U> {
            return when (*self) {
                Option::Some { value } -> Option::Some { value: mapping(Memory::move(value)) },
                else -> Option::None,
            }
        }

        fn and_then<U>(mapping: (T) -> Option<U>) -> Option<U> {
            return when (*self) {
                Option::Some { value } -> mapping(value),
                else -> Option::None,
            }
        }

        fn and_then<U>(mapping: (T) -> Option<U>) move -> Option<U> {
            return when (*self) {
                Option::Some { value } -> mapping(Memory::move(value)),
                else -> Option::None,
            }
        }

        fn filter(predicate: (*T) -> boolean) -> Option<T> {
            return when (*self) {
                Option::Some { value } -> when (predicate(&value)) {
                    true -> Option::Some { value: value },
                    false -> Option::None,
                },
                else -> Option::None,
            }
        }

        fn or_else(fallback: () -> Option<T>) -> Option<T> {
            return when (*self) {
                Option::Some { value } -> Option::Some { value: value },
                else -> fallback(),
            }
        }
}
