import std.ffi.c_fopen
import std.ffi.c_fread
import std.ffi.c_fwrite
import std.ffi.c_fclose
import std.ffi.c_fseek
import std.ffi.c_ftell
import std.ffi.c_remove
import std.ffi.c_access
import std.ffi.c_strlen
import std.ffi.c_mkdir
import std.ffi.c_realpath
import std.ffi.c_opendir
import std.ffi.c_readdir
import std.ffi.c_closedir
import std.ffi.c_clearerr
import std.ffi.zep_errno
import std.text.string.String
import std.text.view.StringView
import std.core.result.Result
import std.core.option.Option
import std.core.unit.Unit
import std.collections.vector.Vector
import std.memory.Memory

public enum FileError {
    OpenFailed
    ReadFailed
    WriteFailed
    SeekFailed
    CloseFailed
    CreateDirectoryFailed
    RemoveFailed
    ReadDirectoryFailed
    ResolvePathFailed

    public:
        fn to_string() -> String {
            return when (*self) {
                FileError::OpenFailed -> String("failed to open file"),
                FileError::ReadFailed -> String("failed to read file"),
                FileError::WriteFailed -> String("failed to write file"),
                FileError::SeekFailed -> String("failed to seek file"),
                FileError::CloseFailed -> String("failed to close file"),
                FileError::CreateDirectoryFailed -> String("failed to create directory"),
                FileError::RemoveFailed -> String("failed to remove file"),
                FileError::ReadDirectoryFailed -> String("failed to read directory"),
                FileError::ResolvePathFailed -> String("failed to resolve path"),
            }
        }
}

public enum AccessMode : i32 {
    Exists = 0
    Read = 4
    Write = 2
    Execute = 1
}

public enum FileType : u8 {
    Directory = 4
    Regular = 8
}

public struct Bytes {
    private:
        var ptr: *mut u8
        var length: i64

    public:
        fn Bytes() -> Bytes {
            return Bytes { ptr: null, length: 0 }
        }

        fn Bytes(length: i64) -> Bytes {
            return Bytes { ptr: Memory::allocate_bytes(length) as *mut u8, length: length }
        }

        fn Bytes(source: &Bytes) -> Bytes {
            var storage = Memory::allocate_bytes(source.length) as *mut u8
            for (var mut index: i64 = 0; index < source.length; index = index + 1) {
                storage[index] = source.ptr[index]
            }
            return Bytes { ptr: storage, length: source.length }
        }

        fn Bytes(source: move Bytes) -> Bytes {
            var storage = source.ptr
            var length = source.length
            source.ptr = null
            source.length = 0
            return Bytes { ptr: storage, length: length }
        }

        fn length() -> i64 { return length }

        fn is_empty() -> boolean { return length == 0 }

        fn as_ptr() -> *void {
            return ptr as *void
        }

        fn as_mut_ptr() mut -> *mut void {
            return ptr as *mut void
        }

        fn ~Bytes() -> void {
            if (ptr != null) {
                Memory::free(ptr as *mut void)
                ptr = null
                length = 0
            }
        }
}

public struct DirectoryEntry {
    public:
        var name: String
        var is_directory: boolean
}

enum TransferDirection : u8 {
    Read = 0
    Write = 1
}

public struct File {
    public:
        static fn read_to_string(path: cstr) -> Result<String, FileError> {
            var result = File::read_bytes(path)
            if (result.is_error()) {
                return Result<String, FileError>::Error { error: result.unwrap_error() }
            }
            var bytes = Memory::move(result).unwrap()
            var view = StringView(bytes.as_ptr() as cstr, bytes.length())
            return Result<String, FileError>::Ok { value: String(&view) }
        }

        static fn write_string(path: cstr, contents: cstr) -> Result<i64, FileError> {
            return File::write_data(path, "wb", contents, contents.length() as i64)
        }

        static fn append_string(path: cstr, contents: cstr) -> Result<i64, FileError> {
            return File::write_data(path, "ab", contents, contents.length() as i64)
        }

        static fn write_bytes(path: cstr, data: *void, length: i64) -> Result<i64, FileError> {
            return File::write_data(path, "wb", data, length)
        }

    private:
        static fn transfer(file: *mut void, data: *mut void, length: i64,
                           direction: TransferDirection) -> boolean {
            var bytes = data as *mut u8
            var mut transferred: i64 = 0
            var error = zep_errno()

            while (transferred < length) {
                error[0] = 0
                var count = if (direction == TransferDirection::Read) {
                    c_fread(&mut bytes[transferred], 1, length - transferred, file)
                } else {
                    c_fwrite(&bytes[transferred], 1, length - transferred, file)
                }

                if (count > 0) {
                    transferred = transferred + count
                }

                if (count == 0 && error[0] == 4) {
                    c_clearerr(file)
                }

                if (count == 0 && error[0] != 4) {
                    return false
                }
            }

            return true
        }

        static fn write_data(path: cstr, mode: cstr, data: *void, length: i64) -> Result<i64, FileError> {
            if (length < 0 || (data == null && length != 0)) {
                return Result<i64, FileError>::Error { error: FileError::WriteFailed }
            }
            var file = c_fopen(path, mode)
            if (file == null) {
                return Result<i64, FileError>::Error { error: FileError::OpenFailed }
            }

            var written = File::transfer(file, data as *mut void, length, TransferDirection::Write)
            var closed = c_fclose(file)

            if (!written) {
                return Result<i64, FileError>::Error { error: FileError::WriteFailed }
            }
            if (closed != 0) {
                return Result<i64, FileError>::Error { error: FileError::CloseFailed }
            }
            return Result<i64, FileError>::Ok { value: length }
        }

    public:
        static fn read_bytes(path: cstr) -> Result<Bytes, FileError> {
            var file = c_fopen(path, "rb")
            if (file == null) {
                return Result<Bytes, FileError>::Error { error: FileError::OpenFailed }
            }

            if (c_fseek(file, 0, 2) != 0) {
                c_fclose(file)
                return Result<Bytes, FileError>::Error { error: FileError::SeekFailed }
            }
            var size = c_ftell(file)
            if (size < 0 || c_fseek(file, 0, 0) != 0) {
                c_fclose(file)
                return Result<Bytes, FileError>::Error { error: FileError::SeekFailed }
            }

            if (size <= (0 as i64)) {
                if (c_fclose(file) != 0) {
                    return Result<Bytes, FileError>::Error { error: FileError::CloseFailed }
                }
                return Result<Bytes, FileError>::Ok { value: Bytes() }
            }

            var mut buffer = Bytes(size)
            var read = File::transfer(file, buffer.as_mut_ptr(), size, TransferDirection::Read)
            var close_result = c_fclose(file)

            if (!read) {
                return Result<Bytes, FileError>::Error { error: FileError::ReadFailed }
            }
            if (close_result != 0) {
                return Result<Bytes, FileError>::Error { error: FileError::CloseFailed }
            }

            return Result<Bytes, FileError>::Ok { value: Memory::move(buffer) }
        }

        static fn create_directory(path: cstr) -> Result<Unit, FileError> {
            var result = c_mkdir(path, 493)
            if (result != 0) {
                return Result<Unit, FileError>::Error { error: FileError::CreateDirectoryFailed }
            }

            return Result<Unit, FileError>::Ok { value: Unit {} }
        }

        static fn create_directories(path: cstr) -> Result<Unit, FileError> {
            var length = c_strlen(path)
            var mut prefix = String()
            var mut index: i64 = 0

            if (length > 0 && path[0] == '/') {
                prefix.push('/')
                index = 1
            }

            while (index <= length) {
                if (index == length || path[index] == '/') {
                    if (prefix.length() > 0 && !prefix.equals("/") && !File::exists(prefix.as_cstr())) {
                        var result = c_mkdir(prefix.as_cstr(), 493)
                        if (result != 0) {
                            return Result<Unit, FileError>::Error { error: FileError::CreateDirectoryFailed }
                        }
                    }
                }

                if (index < length) {
                    prefix.push(path[index])
                }
                index = index + 1
            }

            return Result<Unit, FileError>::Ok { value: Unit {} }
        }

        static fn exists(path: cstr) -> boolean {
            return c_access(path, AccessMode::Exists.value) == 0
        }

        static fn remove(path: cstr) -> Result<Unit, FileError> {
            var result = c_remove(path)
            if (result != 0) {
                return Result<Unit, FileError>::Error { error: FileError::RemoveFailed }
            }
            return Result<Unit, FileError>::Ok { value: Unit {} }
        }
}

public struct Directory {
    public:
        static fn list(path: cstr) -> Result<Vector<DirectoryEntry>, FileError> {
            var directory = c_opendir(path)
            if (directory == null) {
                return Result<Vector<DirectoryEntry>, FileError>::Error {
                    error: FileError::ReadDirectoryFailed
                }
            }

            var mut entries = Vector<DirectoryEntry>()
            var error = zep_errno()
            error[0] = 0
            var mut entry = c_readdir(directory)

            while (entry != null) {
                var name = Directory::entry_name(entry)
                if (!Directory::is_dot_entry(name)) {
                    entries.push(DirectoryEntry { name: String(name), is_directory: Directory::entry_is_directory(entry) })
                }
                error[0] = 0
                entry = c_readdir(directory)
            }

            var read_failed = error[0] != 0
            var closed = c_closedir(directory)
            if (read_failed || closed != 0) {
                return Result<Vector<DirectoryEntry>, FileError>::Error { error: FileError::ReadDirectoryFailed }
            }

            return Result<Vector<DirectoryEntry>, FileError>::Ok { value: Memory::move(entries) }
        }

        @os("linux")
        static fn name_offset() -> i32 {
            return 19
        }

        @os("macos")
        static fn name_offset() -> i32 {
            return 21
        }

        @os("linux")
        static fn type_offset() -> i32 {
            return 18
        }

        @os("macos")
        static fn type_offset() -> i32 {
            return 20
        }

        static fn entry_name(entry: *mut void) -> cstr {
            return (&((entry as *char)[Directory::name_offset()])) as cstr
        }

        static fn entry_is_directory(entry: *mut void) -> boolean {
            var kind = (entry as *u8)[Directory::type_offset()]

            return kind == FileType::Directory.value
        }

        static fn is_dot_entry(name: cstr) -> boolean {
            if (name[0] != '.') {
                return false
            }
            if (name[1] == (0 as char)) {
                return true
            }

            return name[1] == '.' && name[2] == (0 as char)
        }
}

public struct Path {
    public:
        static fn join(left: cstr, right: cstr) -> String {
            var mut result = String(left)
            result.append("/")
            result.append(right)
            return String(result.as_cstr())
        }

        static fn absolute(path: cstr) -> Result<String, FileError> {
            var resolved = c_realpath(path, null)
            if (resolved == null) {
                return Result<String, FileError>::Error { error: FileError::ResolvePathFailed }
            }

            var result = String(resolved)
            Memory::free(resolved as *mut void)

            return Result<String, FileError>::Ok { value: result }
        }

        static fn basename(path: cstr) -> String {
            var length = path.length()
            for (var mut index: i64 = length - 1; index >= 0; index = index - 1) {
                if (path[index] == '/') {
                    return String((&path[index + 1]) as cstr)
                }
            }
            return String(path)
        }

        static fn dirname(path: cstr) -> String {
            var length = path.length()
            for (var mut separator_index: i64 = length - 1; separator_index >= 0;
                 separator_index = separator_index - 1) {
                if (path[separator_index] == '/') {
                    if (separator_index == 0) {
                        return String("/")
                    }

                    var mut result = String()
                    for (var mut index: i64 = 0; index < separator_index; index = index + 1) {
                        result.push(path[index])
                    }

                    return String(result.as_cstr())
                }
            }
            return String(".")
        }
}
