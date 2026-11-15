import std.ffi.handle.Handle
import std.ffi.handle.HandleKind
import std.ffi.c_remove
import std.ffi.zep_errno
import std.core.result.Result
import std.collections.vector.Vector
import std.fs.error.FileError
import std.io.error.IoError
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

        fn finish<T>(value: T) mut -> Result<T, FileError> {
            return when (self->close()) {
                Result::Error { error } -> Result::Error { error: FileError::Close { error: error } },
                Result::Ok -> Result::Ok { value: Memory::move(value) },
            }
        }

        static fn write_data(path: cstr, mode: FileMode, data: *void, length: i64) -> Result<i64, FileError> {
            return when (File::open(path, mode)) {
                Result::Error { error } -> Result::Error { error: error },
                Result::Ok { value } -> when (value.write(data, length)) {
                    Result::Error { error } -> Result::Error { error: FileError::Write { error: error } },
                    Result::Ok { value: count } -> value.finish(count),
                },
            }
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

        static fn open(path: cstr, mode: FileMode) -> Result<File, FileError> {
            return when (Handle::open(path, mode.flags())) {
                Result::Error { error } -> Result::Error { error: FileError::Open { error: error } },
                Result::Ok { value } -> Result::Ok { value: File { handle: Memory::move(value) } },
            }
        }

        static fn open(path: &StringView, mode: FileMode) -> Result<File, FileError> {
            return when (path.contains(0 as char)) {
                true -> Result::Error { error: FileError::Open { error: IoError::InvalidInput } },
                false -> File::open(String(path).as_cstr(), mode),
            }
        }

        static fn open(path: &String, mode: FileMode) -> Result<File, FileError> {
            return File::open(path.as_view(), mode)
        }

        fn as_descriptor() -> i32 {
            return handle.as_descriptor()
        }

        fn read(data: *mut void, length: i64) -> Result<i64, IoError> {
            return handle.read(data, length)
        }

        fn write(value: cstr) -> Result<i64, IoError> {
            return self->write(value, value.length())
        }

        fn write(value: &StringView) -> Result<i64, IoError> {
            return self->write(value.as_ptr(), value.length())
        }

        fn write(value: &String) -> Result<i64, IoError> {
            return self->write(value.as_view())
        }

        fn write(data: *void, length: i64) -> Result<i64, IoError> {
            var bytes = data as *u8
            var mut written: i64 = 0

            while (true) {
                var count = when (handle.write(when (written) {
                    0 -> data,
                    else -> &bytes[written],
                }, length - written)) {
                    Result::Error { error } -> { return Result::Error { error: error } },
                    Result::Ok { value } -> value,
                }

                if (count < 0 || count > length - written) {
                    return Result::Error { error: IoError::Failed }
                }

                written = written + count

                if (written == length) {
                    return Result::Ok { value: written }
                }

                if (count == 0) {
                    return Result::Error { error: IoError::Failed }
                }
            }
        }

        fn seek(offset: i64, origin: SeekOrigin) -> Result<i64, IoError> {
            return handle.seek(offset, origin.value)
        }

        fn position() -> Result<i64, IoError> {
            return self->seek(0, SeekOrigin::Current)
        }

        fn close() mut -> Result<void, IoError> {
            return handle.close()
        }

        static fn read_bytes(path: &StringView) -> Result<Vector<u8>, FileError> {
            var mut file = when (File::open(path, FileMode::Read)) {
                Result::Error { error } -> { return Result::Error { error: error } },
                Result::Ok { value } -> Memory::move(value),
            }
            var mut bytes: Vector<u8> = Vector()
            var mut buffer: u8[4096]

            while (true) {
                var count = when (file.read(&mut buffer, #length(buffer))) {
                    Result::Error { error } -> { return Result::Error { error: FileError::Read { error: error } } },
                    Result::Ok { value } -> value,
                }

                if (count == 0) {
                    return file.finish(Memory::move(bytes))
                }

                bytes.reserve(bytes.length() + count)

                for (var mut index: i64 = 0; index < count; index = index + 1) {
                    bytes.push(buffer[index])
                }
            }
        }

        static fn read_bytes(path: cstr) -> Result<Vector<u8>, FileError> {
            return File::read_bytes(StringView::from_cstr(path))
        }

        static fn read_bytes(path: &String) -> Result<Vector<u8>, FileError> {
            return File::read_bytes(path.as_view())
        }

        static fn read_to_string(path: &StringView) -> Result<String, FileError> {
            return when (File::read_bytes(path)) {
                Result::Error { error } -> Result::Error { error: error },
                Result::Ok { value } -> Result::Ok {
                    value: String(StringView(value.as_ptr() as cstr, value.length()))
                },
            }
        }

        static fn read_to_string(path: cstr) -> Result<String, FileError> {
            return File::read_to_string(StringView::from_cstr(path))
        }

        static fn read_to_string(path: &String) -> Result<String, FileError> {
            return File::read_to_string(path.as_view())
        }

        static fn write_bytes(path: cstr, data: *void, length: i64) -> Result<i64, FileError> {
            return File::write_data(path, FileMode::Write, data, length)
        }

        static fn write_string(path: cstr, contents: cstr) -> Result<i64, FileError> {
            return File::write_data(path, FileMode::Write, contents, contents.length())
        }

        static fn write_string(path: cstr, contents: &StringView) -> Result<i64, FileError> {
            return File::write_data(path, FileMode::Write, contents.as_ptr(), contents.length())
        }

        static fn write_string(path: cstr, contents: &String) -> Result<i64, FileError> {
            return File::write_string(path, contents.as_view())
        }

        static fn append_string(path: cstr, contents: cstr) -> Result<i64, FileError> {
            return File::write_data(path, FileMode::Append, contents, contents.length())
        }

        static fn append_string(path: cstr, contents: &StringView) -> Result<i64, FileError> {
            return File::write_data(path, FileMode::Append, contents.as_ptr(), contents.length())
        }

        static fn append_string(path: cstr, contents: &String) -> Result<i64, FileError> {
            return File::append_string(path, contents.as_view())
        }

        static fn remove(path: cstr) -> Result<void, FileError> {
            return when (c_remove(path)) {
                0 -> Result::Ok,
                else -> Result::Error { error: FileError::Remove { error: *zep_errno() as IoError } },
            }
        }
}
