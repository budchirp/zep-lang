import std.io.console.StandardFile
import std.io.format.FormatArguments
import std.io.format.FormatError
import std.io.format.FormatFailure
import std.io.format.format
import std.io.error.IoError
import std.io.error.IoCode
import std.text.string.String
import std.text.view.StringView

fn write(file: StandardFile, value: &StringView) -> void {
    StandardFile::write(file, value) catch { error: IoError -> 0 as i64, }
}

fn write(file: StandardFile, pattern: &StringView, arguments: &FormatArguments) -> void {
    var text = format(pattern, arguments) catch { error: FormatError -> String(pattern), }

    write(file, text.as_view())
}

public fn print(value: &StringView) -> void {
    write(StandardFile::Output, value)
}

public fn print(value: &String) -> void {
    print(value.as_view())
}

public fn print(value: cstr) -> void {
    print(StringView::from_cstr(value))
}

public fn print(pattern: &StringView, arguments: &FormatArguments) -> void {
    write(StandardFile::Output, pattern, arguments)
}

public fn print(pattern: &String, arguments: &FormatArguments) -> void {
    print(pattern.as_view(), arguments)
}

public fn print(pattern: cstr, arguments: &FormatArguments) -> void {
    print(StringView::from_cstr(pattern), arguments)
}

public fn eprint(value: &StringView) -> void {
    write(StandardFile::Error, value)
}

public fn eprint(value: &String) -> void {
    eprint(value.as_view())
}

public fn eprint(value: cstr) -> void {
    eprint(StringView::from_cstr(value))
}

public fn eprint(pattern: &StringView, arguments: &FormatArguments) -> void {
    write(StandardFile::Error, pattern, arguments)
}

public fn eprint(pattern: &String, arguments: &FormatArguments) -> void {
    eprint(pattern.as_view(), arguments)
}

public fn eprint(pattern: cstr, arguments: &FormatArguments) -> void {
    eprint(StringView::from_cstr(pattern), arguments)
}
