import std.text.String
import std.io.format
import std.io.FormatArguments
import std.io.FormatError
import std.text.ConversionError
import std.text.ConversionFailure

@name("snprintf")
public fn fail_conversion(buffer: cstr, length: i64, pattern: cstr, value: f64) -> i32 {
    return -1
}

public fn main() -> i32 {
    try {
        String::from(1.0)
        return 1
    } catch { error: ConversionError -> {
        if (error.reason != ConversionFailure::InvalidEncoding) {
            return 2
        }
    }, }

    var mut arguments = FormatArguments()

    try {
        arguments.add(1.0)
        return 3
    } catch { error: ConversionError -> {
        if (error.reason != ConversionFailure::InvalidEncoding) {
            return 4
        }
    }, }

    var text = format("", arguments) catch { error: FormatError -> { return 5 }, }

    return when {
        (arguments.is_empty() && text.is_empty()) -> 0,
        else -> 6,
    }
}
