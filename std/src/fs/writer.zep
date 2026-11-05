import std.ffi.c_write
import std.ffi.zep_errno
import std.core.result.Result
import std.io.error.IoError

public enum StandardFile : i32 {
    Output = 1
    Error = 2

    public:
        static fn write(file: StandardFile, value: cstr) -> Result<i64, IoError> {
            return StandardFile::write_bytes(file, value, value.length() as i64)
        }

        static fn write_bytes(file: StandardFile, data: *void, length: i64) -> Result<i64, IoError> {
            if (length < 0 || (data == null && length != 0)) {
                return Result<i64, IoError>::Error { error: IoError { code: 22 } }
            }
            var bytes = data as *u8
            var mut written: i64 = 0
            while (written < length) {
                var result = c_write(file.value, &bytes[written], length - written)
                if (result > 0) {
                    written = written + result
                }
                if (result == 0) {
                    return Result<i64, IoError>::Error { error: IoError { code: 5 } }
                }
                if (result < 0 && *zep_errno() != 4) {
                    return Result<i64, IoError>::Error { error: IoError { code: *zep_errno() } }
                }
            }
            return Result<i64, IoError>::Ok { value: written }
        }
}
