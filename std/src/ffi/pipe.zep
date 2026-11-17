import std.ffi.c_popen
import std.ffi.c_pclose
import std.ffi.c_fileno
import std.ffi.zep_errno
import std.io.error.IoError

public struct Pipe {
    private:
        var handle: *mut void

    public:
        fn Pipe(source: &Pipe) -> Pipe = delete

        fn Pipe(source: move Pipe) -> Pipe {
            var handle = source.handle
            source.handle = null

            return Pipe { handle: handle }
        }

        @throws(IoError)
        static fn open(command: cstr) -> Pipe {
            var handle = c_popen(command, "r")

            return when (handle) {
                null -> throw IoError { value: *zep_errno() },
                else -> Pipe { handle: handle },
            }
        }

        fn as_descriptor() -> i32 {
            return when (handle) {
                null -> -1,
                else -> c_fileno(handle),
            }
        }

        @throws(IoError)
        fn close() mut -> i32 {
            var storage = handle
            handle = null

            if (storage == null) {
                return 0
            }

            var status = c_pclose(storage)

            return when {
                (status < 0) -> throw IoError { value: *zep_errno() },
                else -> status,
            }
        }

        fn ~Pipe() -> void {
            self->close() catch { error: IoError -> 0, }
        }
}
