import std.ffi.c_exit
import std.ffi.c_fprintf
import std.ffi.c_standard_error

public struct CorePanic {
    public:
        static fn panic(message: cstr) -> never {
            c_fprintf(c_standard_error, "%s\n", message)
            c_exit(1)
        }
}
