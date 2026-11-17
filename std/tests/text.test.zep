import std.memory.Memory
import std.text.String
import std.text.StringView
import std.text.string.ParseError
import std.text.string.ParseFailure
import std.collections.vector.CapacityError
import std.testing.Test

test "strings preserve aliased and binary storage through copies and moves" {
    var mut overlap = String("abc")
    overlap.append(overlap.as_view().substring(1, 3).unwrap())

    Test::expect(overlap.equals("abcbc") && String().substring(0, 0).unwrap().is_empty())

    var mut raw: u8[3] = [97 as u8, 0 as u8, 98 as u8]
    var view = StringView((&raw) as cstr, 3)
    var mut text = String(view)
    var copied = text
    text.append(text)

    Test::expect(text.length() == 6 && copied.length() == 3 && copied.get(2).unwrap() == 'b')

    var moved = Memory::move(text)
    text.append("reused")

    Test::expect(moved.length() == 6 && text.equals("reused"))

    var repeated = copied.repeat(3) catch { error: CapacityError -> Test::fail("repeat failed"), }

    Test::expect(repeated.length() == 9 && repeated.get(7).unwrap() == 0 as char)
    Test::expect(copied.replace("", "x").equals(copied))

    var zero = StringView((&raw[1]) as cstr, 1)
    var replacement = StringView::from_cstr("!")

    Test::expect(copied.replace(zero, replacement).equals("a!b"))
    Test::expect(String("MiXeD").to_lowercase().equals("mixed") &&
        String("MiXeD").to_uppercase().equals("MIXED"))
}

test "numeric conversion preserves boundaries and rejects overflow" {
    Test::expect(String::from(-9223372036854775807 as i64 - 1).equals("-9223372036854775808") &&
        String::from(18446744073709551615 as u64).equals("18446744073709551615"))
    try {
        String("9223372036854775808").parse_i64()
        Test::fail("signed overflow was accepted")
    } catch { error: ParseError -> { Test::expect(error.reason == ParseFailure::OutOfRange) }, }

    try {
        String("18446744073709551616").parse_u64()
        Test::fail("unsigned overflow was accepted")
    } catch { error: ParseError -> { Test::expect(error.reason == ParseFailure::OutOfRange) }, }

    try {
        String(" -1").parse_u64()
        Test::fail("negative unsigned value was accepted")
    } catch { error: ParseError -> { Test::expect(error.reason == ParseFailure::OutOfRange) }, }

    try {
        String("1e9999").parse_f64()
        Test::fail("floating point overflow was accepted")
    } catch { error: ParseError -> { Test::expect(error.reason == ParseFailure::OutOfRange) }, }
}
