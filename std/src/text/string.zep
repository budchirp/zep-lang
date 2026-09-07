import std.collections.vector.Vector
import std.core.option.Option
import std.core.result.Result
import std.ffi.c_memcpy
import std.ffi.c_snprintf
import std.ffi.c_strcmp
import std.ffi.c_strlen
import std.ffi.c_strncmp
import std.ffi.c_strtod
import std.ffi.c_strtoll
import std.ffi.c_strtoull
import std.memory.Memory

public enum ParseError {
    InvalidSyntax
    OutOfRange
}

public struct StringView {
    private:
        var ptr: cstr
        var length: i32

    public:
        fn StringView(ptr: cstr, length: i32) -> StringView {
            var mut safe_length = length
            if (safe_length < 0) {
                safe_length = 0
            }
            return StringView { ptr: ptr, length: safe_length }
        }

        static fn from_cstr(ptr: cstr) -> StringView {
            if ((ptr as *mut void) == null) {
                return StringView { ptr: "", length: 0 }
            }

            return StringView { ptr: ptr, length: c_strlen(ptr) as i32 }
        }

        fn length() -> i32 {
            return length
        }

        fn is_empty() -> boolean {
            return length == 0
        }

        fn as_ptr() -> *char {
            return ptr as *char
        }

        fn get(index: i32) -> Option<char> {
            if (index < 0 || index >= length) {
                return Option<char>::None
            }

            return Option<char>::Some { value: ptr[index] }
        }

        fn equals(other: cstr) -> boolean {
            if ((other as *mut void) == null) {
                return false
            }

            var other_length = c_strlen(other) as i32
            return other_length == length && c_strncmp(ptr, other, length as i64) == 0
        }

        fn equals(other: *StringView) -> boolean {
            return other->length() == length &&
                   c_strncmp(ptr, other->as_ptr() as cstr, length as i64) == 0
        }

        fn starts_with(prefix: cstr) -> boolean {
            if ((prefix as *mut void) == null) {
                return false
            }

            var prefix_length = c_strlen(prefix) as i32
            return prefix_length <= length &&
                   c_strncmp(ptr, prefix, prefix_length as i64) == 0
        }

        fn ends_with(suffix: cstr) -> boolean {
            if ((suffix as *mut void) == null) {
                return false
            }

            var suffix_length = c_strlen(suffix) as i32
            if (suffix_length > length) {
                return false
            }

            return c_strncmp((&ptr[length - suffix_length]) as cstr, suffix,
                             suffix_length as i64) == 0
        }

        fn index_of(needle: cstr, from_index: i32) -> Option<i32> {
            if ((needle as *mut void) == null) {
                return Option<i32>::None
            }

            var needle_length = c_strlen(needle) as i32
            var mut start_index = from_index
            if (start_index < 0) {
                start_index = 0
            }

            if (needle_length == 0 && start_index <= length) {
                return Option<i32>::Some { value: start_index }
            }

            var final_index = length - needle_length
            if (needle_length > length || start_index > final_index) {
                return Option<i32>::None
            }

            for (var mut index = start_index; index <= final_index; index = index + 1) {
                if (c_strncmp((&ptr[index]) as cstr, needle, needle_length as i64) == 0) {
                    return Option<i32>::Some { value: index }
                }
            }

            return Option<i32>::None
        }

        fn substring(start_index: i32, end_index: i32) -> Option<StringView> {
            if (start_index < 0 || end_index < start_index || end_index > length) {
                return Option<StringView>::None
            }

            return Option<StringView>::Some {
                value: StringView((&ptr[start_index]) as cstr, end_index - start_index)
            }
        }
}

public struct String {
    private:
        var ptr: *mut char
        var length: i32
        var capacity: i32

        fn ensure_capacity(required_capacity: i32) mut -> void {
            if (required_capacity <= capacity) {
                return
            }

            var mut next_capacity = capacity
            if (next_capacity == 0) {
                next_capacity = 1
            }
            while (next_capacity < required_capacity) {
                next_capacity = next_capacity * 2
            }

            self->reserve(next_capacity)
        }

        static fn parse_end_is_valid(source: cstr, end: cstr) -> boolean {
            return (end as *mut void) != null && end != source && end[0] == (0 as char)
        }

    public:
        fn String() -> String {
            var mut storage = Memory::allocate_bytes(1) as *mut char
            storage[0] = 0 as char
            return String { ptr: storage, length: 0, capacity: 1 }
        }

        fn String(value: cstr) -> String {
            if ((value as *mut void) == null) {
                return String()
            }

            var length = c_strlen(value) as i32
            var capacity = length + 1
            var storage = Memory::allocate_bytes(capacity as i64) as *mut char
            c_memcpy(storage as *mut void, value as *mut void, capacity as i64)
            return String { ptr: storage, length: length, capacity: capacity }
        }

        fn String(value: *StringView) -> String {
            var mut result = String()
            result.reserve(value->length() + 1)
            for (var mut index: i32 = 0; index < value->length(); index = index + 1) {
                result.push(value->get(index).unwrap())
            }

            return result
        }

        static fn from(value: f32) -> String {
            return String::from(value as f64)
        }

        static fn from(value: f64) -> String {
            var buffer = Memory::allocate_bytes(64) as *mut char
            defer Memory::free(buffer as *mut void)
            c_snprintf(buffer as cstr, 64, "%g", value)
            return String(buffer as cstr)
        }

        static fn from(value: i8) -> String { return String::from(value as i64) }
        static fn from(value: i16) -> String { return String::from(value as i64) }
        static fn from(value: i32) -> String { return String::from(value as i64) }

        static fn from(value: i64) -> String {
            var buffer = Memory::allocate_bytes(32) as *mut char
            defer Memory::free(buffer as *mut void)
            c_snprintf(buffer as cstr, 32, "%lld", value)
            return String(buffer as cstr)
        }

        static fn from(value: u8) -> String { return String::from(value as u64) }
        static fn from(value: u16) -> String { return String::from(value as u64) }
        static fn from(value: u32) -> String { return String::from(value as u64) }

        static fn from(value: u64) -> String {
            var buffer = Memory::allocate_bytes(32) as *mut char
            defer Memory::free(buffer as *mut void)
            c_snprintf(buffer as cstr, 32, "%llu", value)
            return String(buffer as cstr)
        }

        static fn from(value: boolean) -> String {
            if (value) {
                return String("true")
            }

            return String("false")
        }

        static fn from(value: char) -> String {
            var mut result = String()
            result.push(value)
            return result
        }

        static fn from(value: cstr) -> String {
            return String(value)
        }

        fn reserve(minimum_capacity: i32) mut -> void {
            if (minimum_capacity <= capacity) {
                return
            }

            var next_ptr = Memory::allocate_bytes(minimum_capacity as i64) as *mut char
            c_memcpy(next_ptr as *mut void, ptr as *mut void, (length + 1) as i64)
            Memory::free(ptr as *mut void)
            ptr = next_ptr
            capacity = minimum_capacity
        }

        fn append(value: cstr) mut -> void {
            if ((value as *mut void) == null) {
                return
            }

            var value_length = c_strlen(value) as i32
            self->ensure_capacity(length + value_length + 1)
            c_memcpy((&ptr[length]) as *mut void, value as *mut void, value_length as i64)
            length = length + value_length
            ptr[length] = 0 as char
        }

        fn append(value: *String) mut -> void {
            self->append(value->as_cstr())
        }

        fn append(value: *StringView) mut -> void {
            self->ensure_capacity(length + value->length() + 1)
            c_memcpy((&ptr[length]) as *mut void, value->as_ptr() as *mut void,
                     value->length() as i64)
            length = length + value->length()
            ptr[length] = 0 as char
        }

        fn push(value: char) mut -> void {
            self->ensure_capacity(length + 2)
            ptr[length] = value
            length = length + 1
            ptr[length] = 0 as char
        }

        fn clear() mut -> void {
            length = 0
            ptr[0] = 0 as char
        }

        fn equals(value: cstr) -> boolean {
            return (value as *mut void) != null && c_strcmp(ptr as cstr, value) == 0
        }

        fn equals(value: *String) -> boolean {
            return self->equals(value->as_cstr())
        }

        fn starts_with(value: cstr) -> boolean {
            var view = self->as_view()
            return view.starts_with(value)
        }

        fn ends_with(value: cstr) -> boolean {
            var view = self->as_view()
            return view.ends_with(value)
        }

        fn index_of(needle: cstr, from_index: i32) -> Option<i32> {
            var view = self->as_view()
            return view.index_of(needle, from_index)
        }

        fn substring(start_index: i32, end_index: i32) -> Option<String> {
            if (start_index < 0 || end_index < start_index || end_index > length) {
                return Option<String>::None
            }

            var mut result = String()
            result.reserve(end_index - start_index + 1)
            for (var mut index = start_index; index < end_index; index = index + 1) {
                result.push(ptr[index])
            }

            return Option<String>::Some { value: result }
        }

        fn split(separator: char) -> Vector<String> {
            var mut result = Vector<String>()
            var mut start_index: i32 = 0

            for (var mut index: i32 = 0; index <= length; index = index + 1) {
                if (index == length || ptr[index] == separator) {
                    result.push(self->substring(start_index, index).unwrap())
                    start_index = index + 1
                }
            }

            return result
        }

        fn trim() -> String {
            var mut start_index: i32 = 0
            var mut end_index = length

            while (start_index < end_index &&
                   (ptr[start_index] == ' ' || ptr[start_index] == '\t' ||
                    ptr[start_index] == '\n' || ptr[start_index] == '\r')) {
                start_index = start_index + 1
            }

            while (end_index > start_index &&
                   (ptr[end_index - 1] == ' ' || ptr[end_index - 1] == '\t' ||
                    ptr[end_index - 1] == '\n' || ptr[end_index - 1] == '\r')) {
                end_index = end_index - 1
            }

            return self->substring(start_index, end_index).unwrap()
        }

        fn to_lowercase() -> String {
            var mut result = String()
            result.reserve(length + 1)

            for (var mut index: i32 = 0; index < length; index = index + 1) {
                var mut character = ptr[index]
                if ((character as i32) >= ('A' as i32) &&
                    (character as i32) <= ('Z' as i32)) {
                    character = ((character as i32) + 32) as char
                }
                result.push(character)
            }

            return result
        }

        fn to_uppercase() -> String {
            var mut result = String()
            result.reserve(length + 1)

            for (var mut index: i32 = 0; index < length; index = index + 1) {
                var mut character = ptr[index]
                if ((character as i32) >= ('a' as i32) &&
                    (character as i32) <= ('z' as i32)) {
                    character = ((character as i32) - 32) as char
                }
                result.push(character)
            }

            return result
        }

        fn replace(needle: cstr, replacement: cstr) -> String {
            if ((needle as *mut void) == null || c_strlen(needle) == (0 as i64)) {
                return String(self->as_cstr())
            }

            var mut result = String()
            var needle_length = c_strlen(needle) as i32
            var mut index: i32 = 0

            while (index < length) {
                if (index + needle_length <= length &&
                    c_strncmp((&ptr[index]) as cstr, needle, needle_length as i64) == 0) {
                    result.append(replacement)
                    index = index + needle_length
                } else {
                    result.push(ptr[index])
                    index = index + 1
                }
            }

            return result
        }

        fn repeat(count: i32) -> String {
            var mut result = String()
            if (count <= 0) {
                return result
            }

            result.reserve((length * count) + 1)
            for (var mut index: i32 = 0; index < count; index = index + 1) {
                result.append(ptr as cstr)
            }

            return result
        }

        fn parse_i32() -> Result<i32, ParseError> {
            var mut end: cstr = null
            var value = c_strtoll(ptr as cstr, &mut end, 10)
            if (!String::parse_end_is_valid(ptr as cstr, end)) {
                return Result<i32, ParseError>::Error { error: ParseError::InvalidSyntax }
            }
            var minimum: i64 = (-2147483647 as i64) - (1 as i64)
            var maximum: i64 = 2147483647 as i64
            if (value < minimum || value > maximum) {
                return Result<i32, ParseError>::Error { error: ParseError::OutOfRange }
            }
            return Result<i32, ParseError>::Ok { value: value as i32 }
        }

        fn parse_i64() -> Result<i64, ParseError> {
            var mut end: cstr = null
            var value = c_strtoll(ptr as cstr, &mut end, 10)
            if (!String::parse_end_is_valid(ptr as cstr, end)) {
                return Result<i64, ParseError>::Error { error: ParseError::InvalidSyntax }
            }
            return Result<i64, ParseError>::Ok { value: value }
        }

        fn parse_u64() -> Result<u64, ParseError> {
            if (length > 0 && ptr[0] == '-') {
                return Result<u64, ParseError>::Error { error: ParseError::OutOfRange }
            }
            var mut end: cstr = null
            var value = c_strtoull(ptr as cstr, &mut end, 10)
            if (!String::parse_end_is_valid(ptr as cstr, end)) {
                return Result<u64, ParseError>::Error { error: ParseError::InvalidSyntax }
            }
            return Result<u64, ParseError>::Ok { value: value }
        }

        fn parse_f64() -> Result<f64, ParseError> {
            var mut end: cstr = null
            var value = c_strtod(ptr as cstr, &mut end)
            if (!String::parse_end_is_valid(ptr as cstr, end)) {
                return Result<f64, ParseError>::Error { error: ParseError::InvalidSyntax }
            }
            return Result<f64, ParseError>::Ok { value: value }
        }

        fn parse_boolean() -> Result<boolean, ParseError> {
            if (self->equals("true")) {
                return Result<boolean, ParseError>::Ok { value: true }
            }
            if (self->equals("false")) {
                return Result<boolean, ParseError>::Ok { value: false }
            }
            return Result<boolean, ParseError>::Error { error: ParseError::InvalidSyntax }
        }

        fn is_empty() -> boolean {
            return length == 0
        }

        fn get(index: i32) -> Option<char> {
            if (index < 0 || index >= length) {
                return Option<char>::None
            }
            return Option<char>::Some { value: ptr[index] }
        }

        fn as_cstr() -> cstr {
            return ptr as cstr
        }

        fn as_view() -> StringView {
            return StringView(ptr as cstr, length)
        }

        fn length() -> i32 {
            return length
        }

        fn capacity() -> i32 {
            return capacity
        }

        fn ~String() -> void {
            if ((ptr as *mut void) != null) {
                Memory::free(ptr as *mut void)
                ptr = null
                length = 0
                capacity = 0
            }
        }
}
