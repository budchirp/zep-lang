import std.ffi.c_exit
import std.ffi.c_write
import std.ffi.c_strlen
import std.ffi.zep_errno

fn write_error(message: cstr) -> void {
    var length = c_strlen(message)
    var mut written: i64 = 0
    while (written < length) {
        var result = c_write(2, &message[written], length - written)
        if (result > 0) {
            written = written + result
        }
        if (result == 0 || (result < 0 && *zep_errno() != 4)) {
            return
        }
    }
}

public fn panic(message: cstr) -> never {
    write_error(message)
    write_error("\n")
    c_exit(1)
}
