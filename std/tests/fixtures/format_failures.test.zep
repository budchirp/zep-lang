import std.text.String
import std.io.format
import std.io.FormatArguments
import std.io.FormatError

@name("snprintf")
public fn fail_conversion(buffer: cstr, length: i64, pattern: cstr, value: f64) -> i32 {
    return -1
}

public fn main() -> i32 {
    if (String::from(1.0).is_ok()) {
        return 1
    }

    var mut arguments = FormatArguments()
    arguments.add(1.0)
    var result = format("{}", arguments)

    if (result.is_ok()) {
        return 2
    }

    var error = result.unwrap_error()

    if (!error.message().equals("cannot format argument at index 0")) {
        return 3
    }

    return when (error) {
        FormatError::Conversion { argument_index, error } -> 0,
        else -> 4,
    }
}
