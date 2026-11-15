import std.fs.file.File
import std.core.result.Result
import std.io.error.IoError
import std.text.string.String
import std.text.view.StringView

public enum StandardFile : i32 {
    Output = 1
    Error = 2

    public:
        static fn write(file: StandardFile, value: cstr) -> Result<i64, IoError> {
            return File::borrow(file.value).write(value)
        }

        static fn write(file: StandardFile, value: &StringView) -> Result<i64, IoError> {
            return File::borrow(file.value).write(value)
        }

        static fn write(file: StandardFile, value: &String) -> Result<i64, IoError> {
            return File::borrow(file.value).write(value)
        }

        static fn write_bytes(file: StandardFile, data: *void, length: i64) -> Result<i64, IoError> {
            return File::borrow(file.value).write(data, length)
        }
}
