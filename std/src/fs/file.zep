import std.ffi.handle.Handle
import std.ffi.handle.HandleKind
import std.ffi.c_remove
import std.ffi.zep_errno
import std.collections.vector.Vector
import std.fs.error.FileError
import std.fs.error.FileOperation
import std.io.error.IoError
import std.io.error.IoCode
import std.memory.Memory
import std.text.string.String
import std.text.view.StringView

public enum FileMode {
    Read
    Write
    Append
    ReadWrite

    public:
        @os("linux")
        fn flags() -> i32 {
            return when (*self) {
                FileMode::Read -> 0,
                FileMode::Write -> 577,
                FileMode::Append -> 1089,
                FileMode::ReadWrite -> 2,
            }
        }

        @os("macos")
        fn flags() -> i32 {
            return when (*self) {
                FileMode::Read -> 0,
                FileMode::Write -> 1537,
                FileMode::Append -> 521,
                FileMode::ReadWrite -> 2,
            }
        }
}

public enum SeekOrigin : i32 {
    Start = 0
    Current = 1
    End = 2
}

public struct File {
    private:
        var handle: Handle

        @throws(FileError)
        fn finish<T>(value: T) mut -> T {
            self->close() catch {
                error: IoError -> throw FileError { operation: FileOperation::Close, error: error },
            }

            return Memory::move(value)
        }

        @throws(FileError)
        static fn write_data(path: cstr, mode: FileMode, data: *void, length: i64) -> i64 {
            var mut file = try File::open(path, mode)
            var count = file.write(data, length) catch {
                error: IoError -> throw FileError { operation: FileOperation::Write, error: error },
            }

            return try file.finish(count)
        }

    public:
        fn File(descriptor: i32) -> File {
            return File { handle: Handle(descriptor, HandleKind::Owned) }
        }

        static fn borrow(descriptor: i32) -> File {
            return File { handle: Handle(descriptor, HandleKind::Borrowed) }
        }

        static fn socket(descriptor: i32) -> File {
            return File { handle: Handle(descriptor, HandleKind::Socket) }
        }

        @throws(FileError)
        static fn open(path: cstr, mode: FileMode) -> File {
            return File { handle: Handle::open(path, mode.flags()) catch {
                error: IoError -> throw FileError { operation: FileOperation::Open, error: error },
            } }
        }

        @throws(FileError)
        static fn open(path: &StringView, mode: FileMode) -> File {
            if (path.contains(0 as char)) {
                throw FileError { operation: FileOperation::Open,
                    error: IoError { value: IoCode::InvalidInput.value } }
            }

            return try File::open(String(path).as_cstr(), mode)
        }

        @throws(FileError)
        static fn open(path: &String, mode: FileMode) -> File {
            return try File::open(path.as_view(), mode)
        }

        fn as_descriptor() -> i32 {
            return handle.as_descriptor()
        }

        @throws(IoError)
        fn read(data: *mut void, length: i64) -> i64 {
            return try handle.read(data, length)
        }

        @throws(IoError)
        fn write(value: cstr) -> i64 {
            return try self->write(value, value.length())
        }

        @throws(IoError)
        fn write(value: &StringView) -> i64 {
            return try self->write(value.as_ptr(), value.length())
        }

        @throws(IoError)
        fn write(value: &String) -> i64 {
            return try self->write(value.as_view())
        }

        @throws(IoError)
        fn write(data: *void, length: i64) -> i64 {
            var bytes = data as *u8
            var mut written: i64 = 0

            while (true) {
                var count = try handle.write(when (written) {
                    0 -> data,
                    else -> &bytes[written],
                }, length - written)

                if (count < 0 || count > length - written) {
                    throw IoError { value: IoCode::Failed.value }
                }

                written = written + count

                if (written == length) {
                    return written
                }

                if (count == 0) {
                    throw IoError { value: IoCode::Failed.value }
                }
            }
        }

        @throws(IoError)
        fn seek(offset: i64, origin: SeekOrigin) -> i64 {
            return try handle.seek(offset, origin.value)
        }

        @throws(IoError)
        fn position() -> i64 {
            return try self->seek(0, SeekOrigin::Current)
        }

        @throws(IoError)
        fn close() mut -> void {
            try handle.close()
        }

        @throws(FileError)
        static fn read_bytes(path: &StringView) -> Vector<u8> {
            var mut file = try File::open(path, FileMode::Read)
            var mut bytes: Vector<u8> = Vector()
            var mut buffer: u8[4096]

            while (true) {
                var count = file.read(&mut buffer, #length(buffer)) catch {
                    error: IoError -> throw FileError { operation: FileOperation::Read, error: error },
                }

                if (count == 0) {
                    return try file.finish(Memory::move(bytes))
                }

                for (var mut index: i64 = 0; index < count; index = index + 1) {
                    bytes.push(buffer[index])
                }
            }
        }

        @throws(FileError)
        static fn read_bytes(path: cstr) -> Vector<u8> {
            return try File::read_bytes(StringView::from_cstr(path))
        }

        @throws(FileError)
        static fn read_bytes(path: &String) -> Vector<u8> {
            return try File::read_bytes(path.as_view())
        }

        @throws(FileError)
        static fn read_to_string(path: &StringView) -> String {
            var bytes = try File::read_bytes(path)

            return String(StringView(bytes.as_ptr() as cstr, bytes.length()))
        }

        @throws(FileError)
        static fn read_to_string(path: cstr) -> String {
            return try File::read_to_string(StringView::from_cstr(path))
        }

        @throws(FileError)
        static fn read_to_string(path: &String) -> String {
            return try File::read_to_string(path.as_view())
        }

        @throws(FileError)
        static fn write_bytes(path: cstr, data: *void, length: i64) -> i64 {
            return try File::write_data(path, FileMode::Write, data, length)
        }

        @throws(FileError)
        static fn write_string(path: cstr, contents: cstr) -> i64 {
            return try File::write_data(path, FileMode::Write, contents, contents.length())
        }

        @throws(FileError)
        static fn write_string(path: cstr, contents: &StringView) -> i64 {
            return try File::write_data(path, FileMode::Write, contents.as_ptr(), contents.length())
        }

        @throws(FileError)
        static fn write_string(path: cstr, contents: &String) -> i64 {
            return try File::write_string(path, contents.as_view())
        }

        @throws(FileError)
        static fn append_string(path: cstr, contents: cstr) -> i64 {
            return try File::write_data(path, FileMode::Append, contents, contents.length())
        }

        @throws(FileError)
        static fn append_string(path: cstr, contents: &StringView) -> i64 {
            return try File::write_data(path, FileMode::Append, contents.as_ptr(), contents.length())
        }

        @throws(FileError)
        static fn append_string(path: cstr, contents: &String) -> i64 {
            return try File::append_string(path, contents.as_view())
        }

        @throws(FileError)
        static fn remove(path: cstr) -> void {
            when (c_remove(path)) {
                0 -> {},
                else -> throw FileError { operation: FileOperation::Remove,
                    error: IoError { value: *zep_errno() } },
            }
        }
}
