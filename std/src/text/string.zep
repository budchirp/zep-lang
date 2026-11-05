import std.collections.vector.Vector
import std.core.option.Option
import std.core.result.Result
import std.ffi.c_memcpy
import std.ffi.c_memmove
import std.ffi.zep_errno
import std.ffi.c_snprintf
import std.ffi.c_strlen
import std.ffi.c_strtod
import std.ffi.c_strtoll
import std.ffi.c_strtoull
import std.memory.Memory
import std.io.panic.panic
import std.text.view.StringView
import std.core.bytes.ByteText

public enum ParseError {
    InvalidSyntax
    OutOfRange
}

public struct String {
    private:
        var ptr: *mut char
        var length: i64
        var capacity: i64

        fn ensure_capacity(required_capacity: i64) mut -> void {
            if (required_capacity < 0 || required_capacity == (9223372036854775807 as i64)) {
                panic("string capacity overflow")
            }
            if (required_capacity <= capacity && ptr != null) {
                return
            }

            var mut next_capacity = capacity
            if (next_capacity == 0) {
                next_capacity = 1
            }
            while (next_capacity < required_capacity) {
                if (next_capacity > (9223372036854775807 as i64) / 2) {
                    next_capacity = required_capacity
                } else {
                    next_capacity = next_capacity * 2
                }
            }

            self->reserve(next_capacity)
        }

        fn transform(mapping: (char) -> char) -> String {
            var mut result = String()
            result.reserve(length)

            for (var mut index: i64 = 0; index < length; index = index + 1) {
                result.push(mapping(ptr[index]))
            }

            return Memory::move(result)
        }

        fn number_result<T>(value: T, end: cstr) -> Result<T, ParseError> {
            if (*zep_errno() == 34) {
                return Result<T, ParseError>::Error { error: ParseError::OutOfRange }
            }

            var source = self->as_cstr()

            if ((end as *void) == null || end == source ||
                (end as u64) - (source as u64) != length as u64) {
                return Result<T, ParseError>::Error { error: ParseError::InvalidSyntax }
            }

            return Result<T, ParseError>::Ok { value: value }
        }

    public:
        fn String() -> String {
            var mut storage = Memory::allocate_bytes(1) as *mut char
            storage[0] = 0 as char
            return String { ptr: storage, length: 0, capacity: 0 }
        }

        fn String(value: cstr) -> String {
            if ((value as *mut void) == null) {
                return String()
            }

            var length = c_strlen(value)
            var capacity = length
            var storage = Memory::allocate_bytes(capacity + 1) as *mut char
            c_memcpy(storage as *mut void, value as *mut void, length + 1)
            return String { ptr: storage, length: length, capacity: capacity }
        }

        fn String(source: &String) -> String {
            var view = source.as_view()
            return String(&view)
        }

        fn String(source: move String) -> String {
            var storage = source.ptr
            var length = source.length
            var capacity = source.capacity
            source.ptr = null
            source.length = 0
            source.capacity = 0
            return String { ptr: storage, length: length, capacity: capacity }
        }

        fn String(value: *StringView) -> String {
            var mut result = String()
            result.reserve(value->length())
            for (var mut index: i64 = 0; index < value->length(); index = index + 1) {
                result.push(value->get(index).unwrap())
            }

            return Memory::move(result)
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

        fn reserve(minimum_capacity: i64) mut -> void {
            if (minimum_capacity < 0 || minimum_capacity == (9223372036854775807 as i64)) {
                panic("invalid string capacity")
            }
            if (minimum_capacity <= capacity) {
                return
            }

            var next_ptr = Memory::allocate_bytes(minimum_capacity + 1) as *mut char
            if (ptr != null) {
                c_memcpy(next_ptr as *mut void, ptr as *mut void, length + 1)
            }
            if (ptr == null) {
                next_ptr[0] = 0 as char
            }
            Memory::free(ptr as *mut void)
            ptr = next_ptr
            capacity = minimum_capacity
        }

        fn append(value: cstr) mut -> void {
            var view = StringView::from_cstr(value)
            self->append(&view)
        }

        fn append(value: *String) mut -> void {
            var view = value->as_view()
            self->append(&view)
        }

        fn append(value: *StringView) mut -> void {
            var source_ptr = value->as_ptr()
            var source_length = value->length()
            var offset = (source_ptr as u64) - (ptr as u64)
            var aliases_storage = (source_ptr as u64) >= (ptr as u64) && offset <= (length as u64)
            self->ensure_capacity(length + source_length)
            var mut source = source_ptr
            if (aliases_storage) {
                source = &ptr[offset as i64]
            }
            c_memmove((&ptr[length]) as *mut void, source, source_length as i64)
            length = length + source_length
            ptr[length] = 0 as char
        }

        fn push(value: char) mut -> void {
            self->ensure_capacity(length + 1)
            ptr[length] = value
            length = length + 1
            ptr[length] = 0 as char
        }

        fn clear() mut -> void {
            length = 0
            if (ptr != null) {
                ptr[0] = 0 as char
            }
        }

        fn equals(value: cstr) -> boolean {
            var view = self->as_view()
            return view.equals(value)
        }

        fn equals(value: *String) -> boolean {
            var left = self->as_view()
            var right = value->as_view()

            return left.equals(&right)
        }

        fn starts_with(value: cstr) -> boolean {
            var view = self->as_view()
            return view.starts_with(value)
        }

        fn ends_with(value: cstr) -> boolean {
            var view = self->as_view()
            return view.ends_with(value)
        }

        fn index_of(needle: cstr, from_index: i64) -> Option<i64> {
            var view = self->as_view()
            return view.index_of(needle, from_index)
        }

        fn substring(start_index: i64, end_index: i64) -> Option<String> {
            if (start_index < 0 || end_index < start_index || end_index > length) {
                return Option<String>::None
            }

            var mut result = String()
            result.reserve(end_index - start_index)
            for (var mut index = start_index; index < end_index; index = index + 1) {
                result.push(ptr[index])
            }

            return Option<String>::Some { value: result }
        }

        fn split(separator: char) -> Vector<String> {
            var mut result = Vector<String>()
            var mut start_index: i64 = 0

            for (var mut index: i64 = 0; index <= length; index = index + 1) {
                if (index == length || ptr[index] == separator) {
                    result.push(self->substring(start_index, index).unwrap())
                    start_index = index + 1
                }
            }

            return result
        }

        fn trim() -> String {
            var mut start_index: i64 = 0
            var mut end_index = length

            while (start_index < end_index && Char::is_whitespace(ptr[start_index])) {
                start_index = start_index + 1
            }

            while (end_index > start_index && Char::is_whitespace(ptr[end_index - 1])) {
                end_index = end_index - 1
            }

            return self->substring(start_index, end_index).unwrap()
        }

        fn to_lowercase() -> String {
            return self->transform(Char::to_lowercase)
        }

        fn to_uppercase() -> String {
            return self->transform(Char::to_uppercase)
        }

        fn replace(needle: cstr, replacement: cstr) -> String {
            var search = StringView::from_cstr(needle)
            var value = StringView::from_cstr(replacement)

            return self->replace(&search, &value)
        }

        fn replace(needle: *StringView, replacement: *StringView) -> String {
            var needle_length = needle->length()

            if (needle_length == 0) {
                var view = self->as_view()
                return String(&view)
            }

            var mut result = String()
            var mut index: i64 = 0

            while (index < length) {
                if (ByteText::prefix((&ptr[index]) as cstr, length - index,
                                     needle->as_ptr() as cstr, needle_length)) {
                    result.append(replacement)
                    index = index + needle_length
                } else {
                    result.push(ptr[index])
                    index = index + 1
                }
            }

            return result
        }

        fn repeat(count: i64) -> String {
            var mut result = String()
            if (count <= 0) {
                return result
            }

            result.reserve(Memory::allocation_size(count, length))
            for (var mut index: i64 = 0; index < count; index = index + 1) {
                result.append(self)
            }

            return result
        }

        fn parse_i32() -> Result<i32, ParseError> {
            var parsed = self->parse_i64()

            if (parsed.is_error()) {
                return Result<i32, ParseError>::Error { error: parsed.unwrap_error() }
            }

            var value = parsed.unwrap()
            var minimum: i64 = (-2147483647 as i64) - (1 as i64)
            var maximum: i64 = 2147483647 as i64
            if (value < minimum || value > maximum) {
                return Result<i32, ParseError>::Error { error: ParseError::OutOfRange }
            }
            return Result<i32, ParseError>::Ok { value: value as i32 }
        }

        fn parse_i64() -> Result<i64, ParseError> {
            var mut end: cstr = null
            var source = self->as_cstr();
            *zep_errno() = 0
            var value = c_strtoll(source, &mut end, 10)

            return self->number_result(value, end)
        }

        fn parse_u64() -> Result<u64, ParseError> {
            var mut start: i64 = 0
            while (start < length && Char::is_whitespace(ptr[start])) {
                start = start + 1
            }
            if (start < length && ptr[start] == '-') {
                return Result<u64, ParseError>::Error { error: ParseError::OutOfRange }
            }
            var mut end: cstr = null
            var source = self->as_cstr();
            *zep_errno() = 0
            var value = c_strtoull(source, &mut end, 10)

            return self->number_result(value, end)
        }

        fn parse_f64() -> Result<f64, ParseError> {
            var mut end: cstr = null
            var source = self->as_cstr();
            *zep_errno() = 0
            var value = c_strtod(source, &mut end)

            return self->number_result(value, end)
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

        fn get(index: i64) -> Option<char> {
            if (index < 0 || index >= length) {
                return Option<char>::None
            }
            return Option<char>::Some { value: ptr[index] }
        }

        fn as_cstr() -> cstr {
            if (ptr == null) {
                return ""
            }
            return ptr as cstr
        }

        fn as_view() -> StringView {
            return StringView(self->as_cstr(), length)
        }

        fn length() -> i64 {
            return length
        }

        fn capacity() -> i64 {
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
