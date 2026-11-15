import std.ffi.c_popen
import std.ffi.c_pclose
import std.ffi.c_fileno
import std.ffi.zep_errno
import std.core.result.Result
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

        static fn open(command: cstr) -> Result<Pipe, IoError> {
            var handle = c_popen(command, "r")

            return when (handle) {
                null -> Result::Error { error: *zep_errno() as IoError },
                else -> Result::Ok { value: Pipe { handle: handle } },
            }
        }

        fn as_descriptor() -> i32 {
            return when (handle) {
                null -> -1,
                else -> c_fileno(handle),
            }
        }

        fn close() mut -> Result<i32, IoError> {
            var storage = handle
            handle = null

            if (storage == null) {
                return Result::Ok { value: 0 }
            }

            var status = c_pclose(storage)

            return when {
                (status < 0) -> Result::Error { error: *zep_errno() as IoError },
                else -> Result::Ok { value: status },
            }
        }

        fn ~Pipe() -> void {
            self->close()
        }
}
