import std.core.marker.Primitive
import std.ffi.c_strlen
import std.ffi.c_strcmp
import std.ffi.c_strncmp

public type i8 = i8
public type i16 = i16
public type i32 = i32
public type i64 = i64
public type u8 = u8
public type u16 = u16
public type u32 = u32
public type u64 = u64
public type f32 = f32
public type f64 = f64
public type boolean = boolean
public type char = char
public type cstr = cstr
public type void = void
public type never = never
public type any = any

public struct Integer<T: i32> : Primitive<T> {
    public:
        var value: T

        static fn minimum_value() -> T {
            if (T is i8) { return (-128) as T }
            if (T is i16) { return (-32768) as T }
            if (T is i32) { return ((-2147483647) - 1) as T }
            if (T is i64) {
                return (((-9223372036854775807 as i64) - (1 as i64)) as T)
            }

            return 0 as T
        }

        static fn maximum_value() -> T {
            if (T is i8) { return 127 as T }
            if (T is i16) { return 32767 as T }
            if (T is i32) { return 2147483647 as T }
            if (T is i64) { return (9223372036854775807 as i64) as T }
            if (T is u8) { return (255 as u8) as T }
            if (T is u16) { return (65535 as u16) as T }
            if (T is u32) { return (4294967295 as u32) as T }
            if (T is u64) { return (18446744073709551615 as u64) as T }

            return 0 as T
        }
    }

public struct Float<T: f64> : Primitive<T> {
    public:
        var value: T
    }

public struct Boolean : Primitive<boolean> {
    public:
        var value: boolean
    }

public struct Char : Primitive<char> {
    public:
        var value: char

        static fn is_digit(value: char) -> boolean {
            return (value as i32) >= ('0' as i32) && (value as i32) <= ('9' as i32)
        }

        static fn is_alpha(value: char) -> boolean {
            var code = value as i32

            return (code >= ('a' as i32) && code <= ('z' as i32)) ||
                   (code >= ('A' as i32) && code <= ('Z' as i32))
        }

        static fn is_alpha_numeric(value: char) -> boolean {
            return Char::is_digit(value) || Char::is_alpha(value)
        }

        static fn is_whitespace(value: char) -> boolean {
            return value == ' ' || value == '\t' || value == '\n' || value == '\r'
        }

        static fn is_hex_digit(value: char) -> boolean {
            var code = value as i32

            return Char::is_digit(value) ||
                   (code >= ('a' as i32) && code <= ('f' as i32)) ||
                   (code >= ('A' as i32) && code <= ('F' as i32))
        }

        static fn to_lowercase(value: char) -> char {
            if ((value as i32) >= ('A' as i32) && (value as i32) <= ('Z' as i32)) {
                return ((value as i32) + 32) as char
            }

            return value
        }

        static fn to_uppercase(value: char) -> char {
            if ((value as i32) >= ('a' as i32) && (value as i32) <= ('z' as i32)) {
                return ((value as i32) - 32) as char
            }

            return value
        }
}

public struct CStr : Primitive<cstr> {
    public:
        var value: cstr

        fn length() -> i32 {
            return c_strlen(value) as i32
        }

        fn is_empty() -> boolean {
            return self->length() == 0
        }

        fn equals(other: cstr) -> boolean {
            return c_strcmp(value, other) == 0
        }

        fn starts_with(prefix: cstr) -> boolean {
            return c_strncmp(value, prefix, c_strlen(prefix)) == 0
        }

        fn contains(needle: cstr) -> boolean {
            var needle_length = c_strlen(needle) as i32
            if (needle_length == 0) {
                return true
            }

            var value_length = self->length()
            if (needle_length > value_length) {
                return false
            }

            var limit = value_length - needle_length

            for (var mut index: i32 = 0; index <= limit; index = index + 1) {
                var mut matched = true

                for (var mut offset: i32 = 0; offset < needle_length; offset = offset + 1) {
                    if (value[index + offset] != needle[offset]) {
                        matched = false
                    }
                }

                if (matched) {
                    return true
                }
            }

            return false
        }
}

public struct Array<T, const N: i32> {
    public:
        var data: T[N]

        fn Array() -> Array<T, N> {
            var data: T[N]
            return Array<T, N> { data: data }
        }

        fn get(index: i32) -> *T {
            if (index < 0 || index >= N) {
                return null
            }

            return &(((&data) as *T)[index])
        }

        fn get_mut(index: i32) mut -> *mut T {
            if (index < 0 || index >= N) {
                return null
            }

            return &mut (((&mut data) as *mut T)[index])
        }

        fn size() -> i32 {
            return N
        }

        fn is_empty() -> boolean {
            return N == 0
        }
}
