import std.core.option.Option
import std.ffi.c_strlen
import std.core.bytes.ByteText

public struct StringView {
    private:
        var ptr: cstr
        var length: i64

    public:
        fn StringView(ptr: cstr, length: i64) -> StringView {
            return when (ptr as *void) {
                null -> StringView { ptr: "", length: 0 },
                else -> StringView { ptr: ptr, length: when {
                    (length < 0) -> 0 as i64,
                    else -> length,
                } },
            }
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
                return Option::None
            }

            return Option::Some { value: ptr[index] }
        }

        fn equals(other: cstr) -> boolean {
            if ((other as *void) == null) {
                return false
            }

            return self->equals(StringView::from_cstr(other))
        }

        fn equals(other: &StringView) -> boolean {
            return ByteText::equal(ptr, length, other.as_ptr() as cstr, other.length())
        }

        fn starts_with(prefix: cstr) -> boolean {
            if ((prefix as *void) == null) {
                return false
            }

            return self->starts_with(StringView::from_cstr(prefix))
        }

        fn starts_with(prefix: &StringView) -> boolean {
            return ByteText::prefix(ptr, length, prefix.as_ptr() as cstr, prefix.length())
        }

        fn ends_with(suffix: cstr) -> boolean {
            if ((suffix as *void) == null) {
                return false
            }

            return self->ends_with(StringView::from_cstr(suffix))
        }

        fn ends_with(suffix: &StringView) -> boolean {
            return ByteText::suffix(ptr, length, suffix.as_ptr() as cstr, suffix.length())
        }

        fn index_of(needle: cstr, from_index: i64) -> Option<i64> {
            if ((needle as *void) == null) {
                return Option::None
            }

            return self->index_of(StringView::from_cstr(needle), from_index)
        }

        fn index_of(needle: &StringView, from_index: i64) -> Option<i64> {
            var index = ByteText::find(ptr, length, needle.as_ptr() as cstr, needle.length(), from_index)

            if (index < 0) {
                return Option::None
            }

            return Option::Some { value: index }
        }

        fn contains(character: char) -> boolean {
            return ByteText::find(ptr, length, (&character) as cstr, 1, 0) >= 0
        }

        fn contains(needle: &StringView) -> boolean {
            return self->index_of(needle, 0).is_some()
        }

        fn contains(needle: cstr) -> boolean {
            return self->index_of(needle, 0).is_some()
        }

        fn substring(start_index: i64, end_index: i64) -> Option<StringView> {
            if (start_index < 0 || end_index < start_index || end_index > length) {
                return Option::None
            }

            return Option::Some {
                value: StringView((&ptr[start_index]) as cstr, end_index - start_index)
            }
        }
}
