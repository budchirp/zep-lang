import std.collections.vector.Vector
import std.collections.vector.CapacityError
import std.collections.vector.CapacityFailure
import std.core.option.Option
import std.core.error.Error
import std.io.panic.panic
import std.ffi.c_memcpy
import std.ffi.c_memmove
import std.ffi.zep_errno
import std.ffi.c_snprintf
import std.ffi.c_strtod
import std.ffi.c_strtoll
import std.ffi.c_strtoull
import std.memory.Memory
import std.text.view.StringView
import std.core.bytes.ByteText

public enum ParseFailure : i32 {
    InvalidSyntax = 0
    OutOfRange = 1
}

public struct ParseError : Error {
    public:
        var reason: ParseFailure
}

public enum ConversionFailure : i32 {
    InvalidEncoding = 0
    TooLong = 1
}

public struct ConversionError : Error {
    public:
        var reason: ConversionFailure
}

public struct String {
    private:
        var ptr: *mut char
        var length: i64
        var capacity: i64

        fn ensure_capacity(required_capacity: i64) mut -> void {
            if (required_capacity >= 0 && required_capacity <= capacity && ptr != null) {
                return
            }

            self->reserve(Memory::grow_capacity(capacity, required_capacity)) catch {
                error: CapacityError -> panic("string capacity overflow"),
            }
        }

        fn transform(mapping: (char) -> char) -> String {
            var mut result = String()
            result.ensure_capacity(length)

            for (var mut index: i64 = 0; index < length; index = index + 1) {
                result.push(mapping(ptr[index]))
            }

            return Memory::move(result)
        }

        @throws(ParseError)
        fn number<T>(value: T, end: cstr) -> T {
            if (*zep_errno() == 34) {
                throw ParseError { reason: ParseFailure::OutOfRange }
            }

            var source = self->as_cstr()

            if ((end as *void) == null || end == source ||
                (end as u64) - (source as u64) != length as u64) {
                throw ParseError { reason: ParseFailure::InvalidSyntax }
            }

            return value
        }

    public:
        fn String() -> String {
            return String { ptr: null, length: 0, capacity: 0 }
        }

        fn String(value: cstr) -> String {
            return String(StringView::from_cstr(value))
        }

        fn String(source: &String) -> String {
            return String(source.as_view())
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

        fn String(value: &StringView) -> String {
            var length = value.length()

            if (length == 0) {
                return String()
            }

            var storage = Memory::allocate_bytes(length + 1) as *mut char
            c_memcpy(storage, value.as_ptr(), length)
            storage[length] = 0 as char

            return String { ptr: storage, length: length, capacity: length }
        }

        @throws(ConversionError)
        static fn from(value: f32) -> String {
            return try String::from(value as f64)
        }

        @throws(ConversionError)
        static fn from(value: f64) -> String {
            var mut buffer: char[64]
            var length = c_snprintf((&mut buffer) as cstr, #length(buffer), "%g", value)

            return when {
                (length < 0) -> throw ConversionError { reason: ConversionFailure::InvalidEncoding },
                (length as i64 >= #length(buffer)) -> throw ConversionError { reason: ConversionFailure::TooLong },
                else -> String(StringView((&buffer) as cstr, length as i64)),
            }
        }

        static fn from<T: i32>(value: T) -> String {
            var negative = value < 0
            var mut number = when (negative) {
                true -> (-((value as i64) + 1)) as u64 + 1,
                false -> value as u64,
            }
            var mut buffer: char[21]
            var mut index = #length(buffer)

            while (number >= 10) {
                index = index - 1
                buffer[index] = (number % 10 + 48) as char
                number = number / 10
            }

            index = index - 1
            buffer[index] = (number + 48) as char

            if (negative) {
                index = index - 1
                buffer[index] = '-'
            }

            return String(StringView((&buffer[index]) as cstr, #length(buffer) - index))
        }

        static fn from(value: boolean) -> String {
            return String(when (value) { true -> "true", false -> "false", })
        }

        static fn from(value: char) -> String {
            var mut result = String()
            result.push(value)

            return Memory::move(result)
        }

        @throws(CapacityError)
        fn reserve(minimum_capacity: i64) mut -> void {
            if (minimum_capacity < 0) {
                throw CapacityError { reason: CapacityFailure::Negative }
            }

            if (minimum_capacity == (9223372036854775807 as i64)) {
                throw CapacityError { reason: CapacityFailure::Overflow }
            }

            if (minimum_capacity <= capacity) {
                return
            }

            var next_ptr = Memory::allocate_bytes(minimum_capacity + 1) as *mut char

            if (ptr != null) {
                c_memcpy(next_ptr, ptr, length)
            }

            next_ptr[length] = 0 as char
            Memory::free(ptr as *mut void)
            ptr = next_ptr
            capacity = minimum_capacity
        }

        fn append(value: cstr) mut -> void {
            self->append(StringView::from_cstr(value))
        }

        fn append(value: &String) mut -> void {
            self->append(value.as_view())
        }

        fn append(value: &StringView) mut -> void {
            var source_ptr = value.as_ptr()
            var source_length = value.length()

            if (source_length == 0) {
                return
            }

            var offset = (source_ptr as u64) - (ptr as u64)
            var aliases_storage = (source_ptr as u64) >= (ptr as u64) && offset <= (length as u64)
            self->ensure_capacity(length + source_length)

            c_memmove(&mut ptr[length],
                when (aliases_storage) {
                    true -> &ptr[offset as i64],
                    false -> source_ptr,
                }, source_length)

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
            return self->as_view().equals(value)
        }

        fn equals(value: &String) -> boolean {
            return self->as_view().equals(value.as_view())
        }

        fn equals(value: &StringView) -> boolean {
            return self->as_view().equals(value)
        }

        fn starts_with(value: cstr) -> boolean {
            return self->as_view().starts_with(value)
        }

        fn starts_with(value: &StringView) -> boolean {
            return self->as_view().starts_with(value)
        }

        fn starts_with(value: &String) -> boolean {
            return self->starts_with(value.as_view())
        }

        fn ends_with(value: cstr) -> boolean {
            return self->as_view().ends_with(value)
        }

        fn ends_with(value: &StringView) -> boolean {
            return self->as_view().ends_with(value)
        }

        fn ends_with(value: &String) -> boolean {
            return self->ends_with(value.as_view())
        }

        fn index_of(needle: cstr, from_index: i64) -> Option<i64> {
            return self->as_view().index_of(needle, from_index)
        }

        fn index_of(needle: &StringView, from_index: i64) -> Option<i64> {
            return self->as_view().index_of(needle, from_index)
        }

        fn index_of(needle: &String, from_index: i64) -> Option<i64> {
            return self->index_of(needle.as_view(), from_index)
        }

        fn substring(start_index: i64, end_index: i64) -> Option<String> {
            return when (self->as_view().substring(start_index, end_index)) {
                Option::Some { value } -> Option::Some { value: String(value) },
                Option::None -> Option::None,
            }
        }

        fn split(separator: char) -> Vector<String> {
            var mut result: Vector<String> = Vector()
            var mut start_index: i64 = 0

            for (var mut index: i64 = 0; index <= length; index = index + 1) {
                if (index == length || ptr[index] == separator) {
                    result.push(self->substring(start_index, index).unwrap())
                    start_index = index + 1
                }
            }

            return Memory::move(result)
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
            return self->replace(StringView::from_cstr(needle), StringView::from_cstr(replacement))
        }

        fn replace(needle: &String, replacement: &String) -> String {
            return self->replace(needle.as_view(), replacement.as_view())
        }

        fn replace(needle: &StringView, replacement: &StringView) -> String {
            var needle_length = needle.length()

            if (needle_length == 0) {
                return String(self->as_view())
            }

            var mut result = String()
            var mut index: i64 = 0

            while (index < length) {
                if (ByteText::prefix((&ptr[index]) as cstr, length - index,
                                     needle.as_ptr() as cstr, needle_length)) {
                    result.append(replacement)
                    index = index + needle_length
                } else {
                    result.push(ptr[index])
                    index = index + 1
                }
            }

            return Memory::move(result)
        }

        @throws(CapacityError)
        fn repeat(count: i64) -> String {
            var mut result = String()

            if (count < 0) {
                throw CapacityError { reason: CapacityFailure::Negative }
            }

            if (count == 0 || length == 0) {
                return Memory::move(result)
            }

            if (length > 0 && count > (9223372036854775807 as i64) / length) {
                throw CapacityError { reason: CapacityFailure::Overflow }
            }

            try result.reserve(count * length)

            for (var mut index: i64 = 0; index < count; index = index + 1) {
                result.append(*self)
            }

            return Memory::move(result)
        }

        @throws(ParseError)
        fn parse_i32() -> i32 {
            var value = try self->parse_i64()

            if (((-2147483647 as i64) - 1) > value || (2147483647 as i64) < value) {
                throw ParseError { reason: ParseFailure::OutOfRange }
            }

            return value as i32
        }

        @throws(ParseError)
        fn parse_i64() -> i64 {
            var mut end: cstr = null;

            *zep_errno() = 0
            var value = c_strtoll(self->as_cstr(), &mut end, 10)

            return try self->number(value, end)
        }

        @throws(ParseError)
        fn parse_u64() -> u64 {
            var mut start: i64 = 0

            while (start < length && Char::is_whitespace(ptr[start])) {
                start = start + 1
            }

            if (start < length && ptr[start] == '-') {
                throw ParseError { reason: ParseFailure::OutOfRange }
            }

            var mut end: cstr = null;

            *zep_errno() = 0
            var value = c_strtoull(self->as_cstr(), &mut end, 10)

            return try self->number(value, end)
        }

        @throws(ParseError)
        fn parse_f64() -> f64 {
            var mut end: cstr = null;

            *zep_errno() = 0
            var value = c_strtod(self->as_cstr(), &mut end)

            return try self->number(value, end)
        }

        @throws(ParseError)
        fn parse_boolean() -> boolean {
            return when {
                (self->equals("true")) -> true,
                (self->equals("false")) -> false,
                else -> throw ParseError { reason: ParseFailure::InvalidSyntax },
            }
        }

        fn is_empty() -> boolean {
            return length == 0
        }

        fn get(index: i64) -> Option<char> {
            return self->as_view().get(index)
        }

        fn as_cstr() -> cstr {
            return when (ptr) { null -> "", else -> ptr as cstr, }
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
            Memory::free(ptr as *mut void)
        }
}
