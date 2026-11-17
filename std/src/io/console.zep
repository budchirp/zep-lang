import std.fs.file.File
import std.io.error.IoError
import std.text.string.String
import std.text.view.StringView

public enum StandardFile : i32 {
    Output = 1
    Error = 2

    public:
        @throws(IoError)
        static fn write(file: StandardFile, value: cstr) -> i64 {
            return try File::borrow(file.value).write(value)
        }

        @throws(IoError)
        static fn write(file: StandardFile, value: &StringView) -> i64 {
            return try File::borrow(file.value).write(value)
        }

        @throws(IoError)
        static fn write(file: StandardFile, value: &String) -> i64 {
            return try File::borrow(file.value).write(value)
        }

        @throws(IoError)
        static fn write_bytes(file: StandardFile, data: *void, length: i64) -> i64 {
            return try File::borrow(file.value).write(data, length)
        }
}
