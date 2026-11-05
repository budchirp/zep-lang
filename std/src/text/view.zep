import std.core.option.Option
import std.ffi.c_strlen
import std.core.bytes.ByteText

public struct StringView {
    private:
        var ptr: cstr
        var length: i64

    public:
        fn StringView(ptr: cstr, length: i64) -> StringView {
            var mut safe_length = length
            var mut source = ptr
            if (safe_length < 0) {
                safe_length = 0
            }
            if ((source as *void) == null) {
                source = ""
                safe_length = 0
            }
            return StringView { ptr: source, length: safe_length }
        }

        static fn from_cstr(ptr: cstr) -> StringView {
            if ((ptr as *void) == null) {
                return StringView { ptr: "", length: 0 }
            }
            return StringView { ptr: ptr, length: c_strlen(ptr) }
        }

        fn length() -> i64 {
            return length
        }

        fn is_empty() -> boolean {
            return length == 0
        }

        fn as_ptr() -> *char {
            return ptr as *char
        }

        fn get(index: i64) -> Option<char> {
            if (index < 0 || index >= length) {
                return Option<char>::None
            }

            return Option<char>::Some { value: ptr[index] }
        }

        fn equals(other: cstr) -> boolean {
            if ((other as *void) == null) {
                return false
            }

            return ByteText::equal(ptr, length, other, c_strlen(other))
        }

        fn equals(other: *StringView) -> boolean {
            return ByteText::equal(ptr, length, other->as_ptr() as cstr, other->length())
        }

        fn starts_with(prefix: cstr) -> boolean {
            if ((prefix as *void) == null) {
                return false
            }

            return ByteText::prefix(ptr, length, prefix, c_strlen(prefix))
        }

        fn ends_with(suffix: cstr) -> boolean {
            if ((suffix as *void) == null) {
                return false
            }

            return ByteText::suffix(ptr, length, suffix, c_strlen(suffix))
        }

        fn index_of(needle: cstr, from_index: i64) -> Option<i64> {
            if ((needle as *void) == null) {
                return Option<i64>::None
            }

            var index = ByteText::find(ptr, length, needle, c_strlen(needle), from_index)

            if (index < 0) {
                return Option<i64>::None
            }

            return Option<i64>::Some { value: index }
        }

        fn substring(start_index: i64, end_index: i64) -> Option<StringView> {
            if (start_index < 0 || end_index < start_index || end_index > length) {
                return Option<StringView>::None
            }
            return Option<StringView>::Some {
                value: StringView((&ptr[start_index]) as cstr, end_index - start_index)
            }
        }
}
