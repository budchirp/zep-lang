import std.ffi.c_opendir
import std.ffi.c_readdir
import std.ffi.c_closedir
import std.ffi.zep_errno
import std.io.error.IoError
import std.io.error.IoCode

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

        @throws(IoError)
        static fn open(path: cstr) -> DirectoryHandle {
            var handle = c_opendir(path)

            return when (handle) {
                null -> throw IoError { value: *zep_errno() },
                else -> DirectoryHandle { handle: handle },
            }
        }

        @throws(IoError)
        fn next() mut -> *mut void {
            if (handle == null) {
                throw IoError { value: IoCode::Closed.value }
            };

            *zep_errno() = 0
            var entry = c_readdir(handle)

            if (entry != null) {
                return entry
            }

            return when (*zep_errno()) {
                0 -> null,
                else -> throw IoError { value: *zep_errno() },
            }
        }

        @throws(IoError)
        fn close() mut -> void {
            var storage = handle
            handle = null

            when {
                (storage == null || c_closedir(storage) == 0) -> {},
                else -> throw IoError { value: *zep_errno() },
            }
        }

        fn ~DirectoryHandle() -> void {
            self->close() catch { error: IoError -> {}, }
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
