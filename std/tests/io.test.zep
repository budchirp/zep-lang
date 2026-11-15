import std.io.FormatArguments
import std.io.format
import std.text.String
import std.text.StringView
import std.testing.Test

test "formatting arguments own temporary and binary text" {
    var mut arguments = FormatArguments()
    do {
        var local = String("retained")
        arguments.add(local)
        0
    }

    Test::expect(format("{}", arguments).unwrap().equals("retained"))
    arguments.clear()

    var mut raw: u8[3] = [97 as u8, 0 as u8, 98 as u8]
    var binary = String(StringView((&raw) as cstr, 3))
    arguments.add(binary)

    Test::expect(format("{}", arguments).unwrap().equals(binary))
}

test "formatting arguments grow clear and retain all numeric overloads" {
    var mut arguments = FormatArguments()
    var mut output = String()

    Test::expect(arguments.is_empty() && arguments.length() == 0 &&
        arguments.append_to(-1, output).is_error() && arguments.append_to(0, output).is_error())

    for (var mut index = 0; index < 40; index = index + 1) {
        arguments.add(index)
    }

    Test::expect(!arguments.is_empty() && arguments.length() == 40 && arguments.append_to(40, output).is_error())

    for (var mut index = 0; index < 40; index = index + 1) {
        output.clear()
        arguments.append_to(index as i64, output).unwrap()
        Test::expect(output.equals(String::from(index)))
    }

    arguments.clear()

    Test::expect(arguments.is_empty() && arguments.length() == 0 && arguments.append_to(0, output).is_error())

    arguments.add("borrowed")
    arguments.add(String("owned"))
    arguments.add(-1 as i8)
    arguments.add(-2 as i16)
    arguments.add(-3 as i32)
    arguments.add(-4 as i64)
    arguments.add(1 as u8)
    arguments.add(2 as u16)
    arguments.add(3 as u32)
    arguments.add(4 as u64)
    arguments.add(1.5 as f32)
    arguments.add(2.5 as f64)
    arguments.add(true)
    arguments.add('Z')

    Test::expect(format("{} {} {} {} {} {} {} {} {} {} {} {} {} {}", arguments).unwrap().equals(
        "borrowed owned -1 -2 -3 -4 1 2 3 4 1.5 2.5 true Z"))
}
