import std.ffi.c_open
import std.ffi.c_close
import std.ffi.c_read
import std.ffi.c_write
import std.ffi.c_send
import std.ffi.c_lseek
import std.ffi.zep_errno
import std.core.result.Result
import std.io.error.IoError

enum Transfer : i32 {
    Read = 0
    Write = 1
}

public enum HandleKind : i32 {
    Owned = 0
    Borrowed = 1
    Socket = 2
}

public struct Handle {
    private:
        var descriptor: i32
        var kind: HandleKind

        @os("linux")
        static fn send_flags() -> i32 {
            return 16384
        }

        @os("macos")
        static fn send_flags() -> i32 {
            return 0
        }

        fn transfer(data: *mut void, length: i64, operation: Transfer) -> Result<i64, IoError> {
            if (descriptor < 0) {
                return Result::Error { error: IoError::Closed }
            }

            if (length < 0 || (data == null && length != 0)) {
                return Result::Error { error: IoError::InvalidInput }
            }

            while (true) {
                var count = when (operation) {
                    Transfer::Read -> c_read(descriptor, data, length),
                    Transfer::Write -> when (kind) {
                        HandleKind::Socket -> c_send(descriptor, data, length, Handle::send_flags()),
                        else -> c_write(descriptor, data, length),
                    },
                }

                if (count >= 0) {
                    return Result::Ok { value: count }
                }

                if (*zep_errno() != IoError::Interrupted.value) {
                    return Result::Error { error: *zep_errno() as IoError }
                }
            }
        }

    public:
        fn Handle(descriptor: i32, kind: HandleKind) -> Handle {
            return Handle { descriptor: descriptor, kind: kind }
        }

        fn Handle(source: &Handle) -> Handle = delete

        fn Handle(source: move Handle) -> Handle {
            var descriptor = source.descriptor
            source.descriptor = -1

            return Handle(descriptor, source.kind)
        }

        static fn open(path: cstr, flags: i32) -> Result<Handle, IoError> {
            var descriptor = c_open(path, flags, 438 as u32)

            return when {
                (descriptor < 0) -> Result::Error { error: *zep_errno() as IoError },
                else -> Result::Ok { value: Handle(descriptor, HandleKind::Owned) },
            }
        }

        fn as_descriptor() -> i32 {
            return descriptor
        }

        fn read(data: *mut void, length: i64) -> Result<i64, IoError> {
            return self->transfer(data, length, Transfer::Read)
        }

        fn write(data: *void, length: i64) -> Result<i64, IoError> {
            return self->transfer(data as *mut void, length, Transfer::Write)
        }

        fn seek(offset: i64, origin: i32) -> Result<i64, IoError> {
            var position = c_lseek(descriptor, offset, origin)

            return when {
                (position < 0) -> Result::Error { error: *zep_errno() as IoError },
                else -> Result::Ok { value: position },
            }
        }

        fn close() mut -> Result<void, IoError> {
            var descriptor = self->descriptor
            self->descriptor = -1

            return when {
                (kind == HandleKind::Borrowed || descriptor < 0 || c_close(descriptor) == 0) -> Result::Ok,
                else -> Result::Error { error: *zep_errno() as IoError },
            }
        }

        fn ~Handle() -> void {
            self->close()
        }
}
