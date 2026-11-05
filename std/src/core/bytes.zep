import std.ffi.c_memcmp

public struct ByteText {
    public:
        static fn equal(left: cstr, left_length: i64, right: cstr, right_length: i64) -> boolean {
            return left_length == right_length && c_memcmp(left, right, left_length) == 0
        }

        static fn prefix(source: cstr, length: i64, value: cstr, value_length: i64) -> boolean {
            return value_length <= length && ByteText::equal(source, value_length, value, value_length)
        }

        static fn suffix(source: cstr, length: i64, value: cstr, value_length: i64) -> boolean {
            if (value_length > length) {
                return false
            }

            return ByteText::equal((&source[length - value_length]) as cstr, value_length, value, value_length)
        }

        static fn find(source: cstr, length: i64, needle: cstr, needle_length: i64, start: i64) -> i64 {
            var mut index = start

            if (index < 0) {
                index = 0
            }

            if (needle_length == 0 && index <= length) {
                return index
            }

            var limit = length - needle_length

            while (index <= limit) {
                if (ByteText::equal((&source[index]) as cstr, needle_length, needle, needle_length)) {
                    return index
                }

                index = index + 1
            }

            return -1
        }
}
