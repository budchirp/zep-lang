import std.ffi.c_open
import std.ffi.c_close
import std.ffi.c_read
import std.ffi.c_write
import std.ffi.c_send
import std.ffi.c_lseek
import std.ffi.zep_errno
import std.io.error.IoError
import std.io.error.IoCode

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

        @throws(IoError)
        fn transfer(data: *mut void, length: i64, operation: Transfer) -> i64 {
            if (descriptor < 0) {
                throw IoError { value: IoCode::Closed.value }
            }

            if (length < 0 || (data == null && length != 0)) {
                throw IoError { value: IoCode::InvalidInput.value }
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
                    return count
                }

                if (*zep_errno() != IoCode::Interrupted.value) {
                    throw IoError { value: *zep_errno() }
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

        @throws(IoError)
        static fn open(path: cstr, flags: i32) -> Handle {
            var descriptor = c_open(path, flags, 438 as u32)

            return when {
                (descriptor < 0) -> throw IoError { value: *zep_errno() },
                else -> Handle(descriptor, HandleKind::Owned),
            }
        }

        fn as_descriptor() -> i32 {
            return descriptor
        }

        @throws(IoError)
        fn read(data: *mut void, length: i64) -> i64 {
            return try self->transfer(data, length, Transfer::Read)
        }

        @throws(IoError)
        fn write(data: *void, length: i64) -> i64 {
            return try self->transfer(data as *mut void, length, Transfer::Write)
        }

        @throws(IoError)
        fn seek(offset: i64, origin: i32) -> i64 {
            var position = c_lseek(descriptor, offset, origin)

            return when {
                (position < 0) -> throw IoError { value: *zep_errno() },
                else -> position,
            }
        }

        @throws(IoError)
        fn close() mut -> void {
            var descriptor = self->descriptor
            self->descriptor = -1

            when {
                (kind == HandleKind::Borrowed || descriptor < 0 || c_close(descriptor) == 0) -> {},
                else -> throw IoError { value: *zep_errno() },
            }
        }

        fn ~Handle() -> void {
            self->close() catch { error: IoError -> {}, }
        }
}
