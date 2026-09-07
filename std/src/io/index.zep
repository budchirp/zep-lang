import std.fs.writer.StandardFile
import std.io.format.FormatArguments
import std.io.format.sprint
import std.io.panic.panic
import std.text.string.String

fn formatted(format: cstr, arguments: *FormatArguments) -> String {
    var result = sprint(format, arguments)
    if (result.is_error()) {
        var error = result.unwrap_error().message()
        panic(error.as_cstr())
    }

    return result.unwrap()
}

public fn print(format: cstr, arguments: *FormatArguments) -> void {
    var output = formatted(format, arguments)
    StandardFile::write(StandardFile::Output, output.as_cstr())
}

public fn print(format: cstr) -> void {
    var arguments = FormatArguments()
    print(format, &arguments)
}

public fn eprint(format: cstr, arguments: *FormatArguments) -> void {
    var output = formatted(format, arguments)
    StandardFile::write(StandardFile::Error, output.as_cstr())
}

public fn eprint(format: cstr) -> void {
    var arguments = FormatArguments()
    eprint(format, &arguments)
}
