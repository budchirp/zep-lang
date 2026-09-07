import std.ffi.c_write

public enum StandardFile : i32 {
    Output = 1
    Error = 2

    public:
        static fn write(file: StandardFile, value: cstr) -> void {
            c_write(file.value, value as *void, value.length() as i64)
        }
}
