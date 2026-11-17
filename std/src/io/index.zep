import std.io.console.StandardFile
import std.io.format.FormatArguments
import std.io.format.FormatError
import std.io.format.FormatFailure
import std.io.format.format
import std.io.error.IoError
import std.io.error.IoCode
import std.text.string.String
import std.text.view.StringView

@throws(FormatError, IoError)
fn write(file: StandardFile, pattern: &StringView, arguments: &FormatArguments) -> i64 {
    var text = try format(pattern, arguments)

    return try StandardFile::write(file, text)
}

@throws(IoError)
public fn print(value: &StringView) -> i64 {
    return try StandardFile::write(StandardFile::Output, value)
}

@throws(IoError)
public fn print(value: &String) -> i64 {
    return try print(value.as_view())
}

@throws(IoError)
public fn print(value: cstr) -> i64 {
    return try print(StringView::from_cstr(value))
}

@throws(FormatError, IoError)
public fn print(pattern: &StringView, arguments: &FormatArguments) -> i64 {
    return try write(StandardFile::Output, pattern, arguments)
}

@throws(FormatError, IoError)
public fn print(pattern: &String, arguments: &FormatArguments) -> i64 {
    return try print(pattern.as_view(), arguments)
}

@throws(FormatError, IoError)
public fn print(pattern: cstr, arguments: &FormatArguments) -> i64 {
    return try print(StringView::from_cstr(pattern), arguments)
}

@throws(IoError)
public fn eprint(value: &StringView) -> i64 {
    return try StandardFile::write(StandardFile::Error, value)
}

@throws(IoError)
public fn eprint(value: &String) -> i64 {
    return try eprint(value.as_view())
}

@throws(IoError)
public fn eprint(value: cstr) -> i64 {
    return try eprint(StringView::from_cstr(value))
}

@throws(FormatError, IoError)
public fn eprint(pattern: &StringView, arguments: &FormatArguments) -> i64 {
    return try write(StandardFile::Error, pattern, arguments)
}

@throws(FormatError, IoError)
public fn eprint(pattern: &String, arguments: &FormatArguments) -> i64 {
    return try eprint(pattern.as_view(), arguments)
}

@throws(FormatError, IoError)
public fn eprint(pattern: cstr, arguments: &FormatArguments) -> i64 {
    return try eprint(StringView::from_cstr(pattern), arguments)
}
