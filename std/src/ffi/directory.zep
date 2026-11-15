import std.ffi.c_opendir
import std.ffi.c_readdir
import std.ffi.c_closedir
import std.ffi.zep_errno
import std.core.result.Result
import std.io.error.IoError

public struct DirectoryHandle {
    private:
        var handle: *mut void

    public:
        fn DirectoryHandle(source: &DirectoryHandle) -> DirectoryHandle = delete

        fn DirectoryHandle(source: move DirectoryHandle) -> DirectoryHandle {
            var handle = source.handle
            source.handle = null

            return DirectoryHandle { handle: handle }
        }

        static fn open(path: cstr) -> Result<DirectoryHandle, IoError> {
            var handle = c_opendir(path)

            return when (handle) {
                null -> Result::Error { error: *zep_errno() as IoError },
                else -> Result::Ok { value: DirectoryHandle { handle: handle } },
            }
        }

        fn next() mut -> Result<*mut void, IoError> {
            if (handle == null) {
                return Result::Error { error: IoError::Closed }
            };

            *zep_errno() = 0
            var entry = c_readdir(handle)

            if (entry != null) {
                return Result::Ok { value: entry }
            }

            return when (*zep_errno()) {
                0 -> Result::Ok { value: null },
                else -> Result::Error { error: *zep_errno() as IoError },
            }
        }

        fn close() mut -> Result<void, IoError> {
            var storage = handle
            handle = null

            return when {
                (storage == null || c_closedir(storage) == 0) -> Result::Ok,
                else -> Result::Error { error: *zep_errno() as IoError },
            }
        }

        fn ~DirectoryHandle() -> void {
            self->close()
        }

        @os("linux")
        static fn name(entry: *void) -> cstr {
            return (&(entry as *char)[19]) as cstr
        }

        @os("macos")
        static fn name(entry: *void) -> cstr {
            return (&(entry as *char)[21]) as cstr
        }

        @os("linux")
        static fn is_directory(entry: *void) -> boolean {
            return (entry as *u8)[18] == (4 as u8)
        }

        @os("macos")
        static fn is_directory(entry: *void) -> boolean {
            return (entry as *u8)[20] == (4 as u8)
        }
}
