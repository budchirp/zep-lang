import std.ffi.c_exit
import std.fs.writer.StandardFile

public fn panic(message: cstr) -> never {
    StandardFile::write(StandardFile::Error, message)
    StandardFile::write(StandardFile::Error, "\n")
    c_exit(1)
}
