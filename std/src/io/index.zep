import std.fs.writer.StandardFile
import std.io.format.FormatArguments
import std.io.format.FormatError
import std.io.format.format
import std.io.error.IoError
import std.core.result.Result

public enum PrintError {
    Formatting { error: FormatError }
    Write { error: IoError }
}

fn write(file: StandardFile, pattern: cstr, arguments: *FormatArguments) -> Result<i64, PrintError> {
    var formatted = format(pattern, arguments)
    if (formatted.is_error()) {
        return Result<i64, PrintError>::Error { error: PrintError::Formatting { error: formatted.unwrap_error() } }
    }
    var output = formatted.unwrap()
    var result = StandardFile::write_bytes(file, output.as_cstr(), output.length() as i64)
    if (result.is_error()) {
        return Result<i64, PrintError>::Error { error: PrintError::Write { error: result.unwrap_error() } }
    }
    return Result<i64, PrintError>::Ok { value: result.unwrap() }
}

public fn print(pattern: cstr, arguments: *FormatArguments) -> Result<i64, PrintError> {
    return write(StandardFile::Output, pattern, arguments)
}

public fn print(pattern: cstr) -> Result<i64, PrintError> {
    var arguments = FormatArguments()
    return print(pattern, &arguments)
}

public fn eprint(pattern: cstr, arguments: *FormatArguments) -> Result<i64, PrintError> {
    return write(StandardFile::Error, pattern, arguments)
}

public fn eprint(pattern: cstr) -> Result<i64, PrintError> {
    var arguments = FormatArguments()
    return eprint(pattern, &arguments)
}
