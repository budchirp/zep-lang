import std.ffi.directory.DirectoryHandle
import std.ffi.c_mkdir
import std.core.option.Option
import std.collections.vector.Vector
import std.fs.error.FileError
import std.fs.error.FileOperation
import std.io.error.IoError
import std.io.error.IoCode
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
        @throws(FileError)
        static fn open(path: cstr) -> Directory {
            return Directory { handle: DirectoryHandle::open(path) catch {
                error: IoError -> throw FileError { operation: FileOperation::ReadDirectory, error: error },
            } }
        }

        @throws(FileError)
        static fn open(path: &StringView) -> Directory {
            if (path.contains(0 as char)) {
                throw FileError { operation: FileOperation::ReadDirectory,
                    error: IoError { value: IoCode::InvalidInput.value } }
            }

            return try Directory::open(String(path).as_cstr())
        }

        @throws(FileError)
        static fn open(path: &String) -> Directory {
            return try Directory::open(path.as_view())
        }

        @throws(FileError)
        fn next() mut -> Option<DirectoryEntry> {
            while (true) {
                var entry = handle.next() catch {
                    error: IoError -> throw FileError { operation: FileOperation::ReadDirectory, error: error },
                }

                if (entry == null) {
                    return Option::None
                }

                var name = DirectoryHandle::name(entry)

                if (!name.equals(".") && !name.equals("..")) {
                    return Option::Some { value: DirectoryEntry {
                        name: String(name), is_directory: DirectoryHandle::is_directory(entry)
                    } }
                }
            }
        }

        @throws(FileError)
        fn close() mut -> void {
            handle.close() catch {
                error: IoError -> throw FileError { operation: FileOperation::Close, error: error },
            }
        }

        @throws(FileError)
        static fn list(path: cstr) -> Vector<DirectoryEntry> {
            var mut directory = try Directory::open(path)
            var mut entries: Vector<DirectoryEntry> = Vector()

            while (true) {
                var entry = try directory.next()

                when (entry) {
                    Option::Some { value } -> entries.push(Memory::move(value)),
                    Option::None -> {
                        try directory.close()

                        return Memory::move(entries)
                    },
                }
            }
        }

        @throws(FileError)
        static fn create(path: cstr) -> void {
            when (c_mkdir(path, 493)) {
                0 -> {},
                else -> throw FileError { operation: FileOperation::CreateDirectory,
                    error: IoError { value: *zep_errno() } },
            }
        }

        @throws(FileError)
        static fn create_all(path: cstr) -> void {
            var mut prefix = String()
            var length = path.length()

            for (var mut index: i64 = 0; index <= length; index = index + 1) {
                if (index == length || path[index] == '/') {
                    if (!prefix.is_empty()) {
                        Directory::create(prefix.as_cstr()) catch {
                            error: FileError -> {
                                if (error.error.value != IoCode::Exists.value) {
                                    throw error
                                }

                                DirectoryHandle::open(prefix.as_cstr()) catch {
                                    error: IoError -> throw FileError {
                                        operation: FileOperation::CreateDirectory, error: error
                                    },
                                }

                                do {}
                            },
                        }
                    }
                }

                if (index < length) {
                    prefix.push(path[index])
                }
            }

        }
}
