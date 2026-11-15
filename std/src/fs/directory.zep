import std.ffi.directory.DirectoryHandle
import std.ffi.c_mkdir
import std.core.result.Result
import std.core.option.Option
import std.collections.vector.Vector
import std.fs.error.FileError
import std.io.error.IoError
import std.ffi.zep_errno
import std.memory.Memory
import std.text.string.String
import std.text.view.StringView

public struct DirectoryEntry {
    public:
        var name: String
        var is_directory: boolean
}

public struct Directory {
    private:
        var handle: DirectoryHandle

    public:
        static fn open(path: cstr) -> Result<Directory, FileError> {
            return when (DirectoryHandle::open(path)) {
                Result::Error { error } -> Result::Error { error: FileError::ReadDirectory { error: error } },
                Result::Ok { value } -> Result::Ok { value: Directory { handle: Memory::move(value) } },
            }
        }

        static fn open(path: &StringView) -> Result<Directory, FileError> {
            return when (path.contains(0 as char)) {
                true -> Result::Error { error: FileError::ReadDirectory { error: IoError::InvalidInput } },
                false -> Directory::open(String(path).as_cstr()),
            }
        }

        static fn open(path: &String) -> Result<Directory, FileError> {
            return Directory::open(path.as_view())
        }

        fn next() mut -> Result<Option<DirectoryEntry>, FileError> {
            while (true) {
                var entry = when (handle.next()) {
                    Result::Error { error } -> { return Result::Error { error: FileError::ReadDirectory { error: error } } },
                    Result::Ok { value } -> value,
                }

                if (entry == null) {
                    return Result::Ok { value: Option::None }
                }

                var name = DirectoryHandle::name(entry)

                if (!name.equals(".") && !name.equals("..")) {
                    return Result::Ok { value: Option::Some { value: DirectoryEntry {
                        name: String(name), is_directory: DirectoryHandle::is_directory(entry)
                    } } }
                }
            }
        }

        fn close() mut -> Result<void, FileError> {
            return when (handle.close()) {
                Result::Error { error } -> Result::Error { error: FileError::Close { error: error } },
                Result::Ok -> Result::Ok,
            }
        }

        static fn list(path: cstr) -> Result<Vector<DirectoryEntry>, FileError> {
            var mut directory = when (Directory::open(path)) {
                Result::Error { error } -> { return Result::Error { error: error } },
                Result::Ok { value } -> Memory::move(value),
            }
            var mut entries: Vector<DirectoryEntry> = Vector()

            while (true) {
                var entry = when (directory.next()) {
                    Result::Error { error } -> { return Result::Error { error: error } },
                    Result::Ok { value } -> Memory::move(value),
                }

                when (entry) {
                    Option::Some { value } -> entries.push(Memory::move(value)),
                    Option::None -> {
                        return when (directory.close()) {
                            Result::Error { error } -> Result::Error { error: error },
                            Result::Ok -> Result::Ok { value: Memory::move(entries) },
                        }
                    },
                }
            }
        }

        static fn create(path: cstr) -> Result<void, FileError> {
            return when (c_mkdir(path, 493)) {
                0 -> Result::Ok,
                else -> Result::Error { error: FileError::CreateDirectory { error: *zep_errno() as IoError } },
            }
        }

        static fn create_all(path: cstr) -> Result<void, FileError> {
            var mut prefix = String()
            var length = path.length()

            for (var mut index: i64 = 0; index <= length; index = index + 1) {
                if (index == length || path[index] == '/') {
                    if (!prefix.is_empty()) {
                        when (Directory::create(prefix.as_cstr())) {
                            Result::Ok -> {},
                            Result::Error { error } -> {
                                if (error.error.value != IoError::Exists.value) {
                                    return Result::Error { error: error }
                                }

                                when (DirectoryHandle::open(prefix.as_cstr())) {
                                    Result::Error { error } -> {
                                        return Result::Error { error: FileError::CreateDirectory { error: error } }
                                    },
                                    Result::Ok { value } -> {},
                                }
                            },
                        }
                    }
                }

                if (index < length) {
                    prefix.push(path[index])
                }
            }

            return Result::Ok
        }
}
