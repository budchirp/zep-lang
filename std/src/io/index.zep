import std.io.console.StandardFile
import std.io.format.FormatArguments
import std.io.format.FormatError
import std.io.format.format
import std.io.error.IoError
import std.core.result.Result
import std.text.string.String
import std.text.view.StringView

public enum PrintError {
    Formatting { error: FormatError }
    Write { error: IoError }
}

fn output(file: StandardFile, value: &StringView) -> Result<i64, PrintError> {
    return when (StandardFile::write(file, value)) {
        Result::Error { error } -> Result::Error { error: PrintError::Write { error: error } },
        Result::Ok { value } -> Result::Ok { value: value },
    }
}

fn write(file: StandardFile, pattern: &StringView, arguments: &FormatArguments) -> Result<i64, PrintError> {
    return when (format(pattern, arguments)) {
        Result::Error { error } -> Result::Error { error: PrintError::Formatting { error: error } },
        Result::Ok { value } -> output(file, value.as_view()),
    }
}

public fn print(value: &StringView) -> Result<i64, PrintError> {
    return output(StandardFile::Output, value)
}

public fn print(value: &String) -> Result<i64, PrintError> {
    return print(value.as_view())
}

public fn print(value: cstr) -> Result<i64, PrintError> {
    return print(StringView::from_cstr(value))
}

public fn print(pattern: &StringView, arguments: &FormatArguments) -> Result<i64, PrintError> {
    return write(StandardFile::Output, pattern, arguments)
}

public fn print(pattern: &String, arguments: &FormatArguments) -> Result<i64, PrintError> {
    return print(pattern.as_view(), arguments)
}

public fn print(pattern: cstr, arguments: &FormatArguments) -> Result<i64, PrintError> {
    return print(StringView::from_cstr(pattern), arguments)
}

public fn eprint(value: &StringView) -> Result<i64, PrintError> {
    return output(StandardFile::Error, value)
}

public fn eprint(value: &String) -> Result<i64, PrintError> {
    return eprint(value.as_view())
}

public fn eprint(value: cstr) -> Result<i64, PrintError> {
    return eprint(StringView::from_cstr(value))
}

public fn eprint(pattern: &StringView, arguments: &FormatArguments) -> Result<i64, PrintError> {
    return write(StandardFile::Error, pattern, arguments)
}

public fn eprint(pattern: &String, arguments: &FormatArguments) -> Result<i64, PrintError> {
    return eprint(pattern.as_view(), arguments)
}

public fn eprint(pattern: cstr, arguments: &FormatArguments) -> Result<i64, PrintError> {
    return eprint(StringView::from_cstr(pattern), arguments)
}
