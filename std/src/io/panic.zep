import std.ffi.c_exit
import std.ffi.c_write
import std.ffi.c_strlen
import std.ffi.zep_errno

public fn panic(message: cstr) -> never {
    var length = c_strlen(message)
    var mut written: i64 = 0

    while (written <= length) {
        var count = when {
            (written < length) -> c_write(2, &message[written], length - written),
            else -> c_write(2, "\n", 1),
        }

        written = when {
            (count > 0) -> written + count,
            (count < 0 && *zep_errno() == 4) -> written,
            (written < length) -> length,
            else -> length + 1,
        }
    }

    c_exit(1)
}
