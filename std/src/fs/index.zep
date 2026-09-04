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
import std.text.string.String
import std.core.result.Result
import std.core.option.Option
import std.collections.vector.Vector
import std.memory.Memory

public enum FileError {
    OpenFailed
    ReadFailed
    WriteFailed
    CreateDirectoryFailed
    RemoveFailed
    ReadDirectoryFailed
    ResolvePathFailed
    CopyFailed

    public:
        fn to_string() -> String {
            return when (*self) {
                FileError::OpenFailed -> String("failed to open file"),
                FileError::ReadFailed -> String("failed to read file"),
                FileError::WriteFailed -> String("failed to write file"),
                FileError::CreateDirectoryFailed -> String("failed to create directory"),
                FileError::RemoveFailed -> String("failed to remove file"),
                FileError::ReadDirectoryFailed -> String("failed to read directory"),
                FileError::ResolvePathFailed -> String("failed to resolve path"),
                FileError::CopyFailed -> String("failed to copy path"),
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
    public:
        var ptr: *mut u8
        var length: i64

        fn Bytes() -> Bytes {
            return Bytes { ptr: null, length: 0 }
        }

        fn as_ptr() -> *mut void {
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

public struct File {
    public:
        static fn read_to_string(path: cstr) -> Result<String, FileError> {
            var file = c_fopen(path, "r")
            if (file == null) {
                return Result<String, FileError>::Error { error: FileError::OpenFailed }
            }

            c_fseek(file, 0, 2)
            var size = c_ftell(file)
            c_fseek(file, 0, 0)

            if (size <= (0 as i64)) {
                c_fclose(file)
                return Result<String, FileError>::Ok { value: String() }
            }

            var mut buffer = Memory::allocate_bytes(size + (1 as i64)) as *mut char
            var read = c_fread(buffer as *mut void, 1 as i64, size, file)
            c_fclose(file)

            if (read != size) {
                Memory::free(buffer as *mut void)
                return Result<String, FileError>::Error { error: FileError::ReadFailed }
            }

            buffer[size as i32] = 0 as char
            var result = String(buffer as cstr)
            Memory::free(buffer as *mut void)

            return Result<String, FileError>::Ok { value: result }
        }

        static fn write_string(path: cstr, contents: cstr) -> Result<i32, FileError> {
            var file = c_fopen(path, "w")
            if (file == null) {
                return Result<i32, FileError>::Error { error: FileError::OpenFailed }
            }

            var length = contents.length() as i64
            var written = c_fwrite(contents as *void, 1 as i64, length, file)
            c_fclose(file)

            if (written != length) {
                return Result<i32, FileError>::Error { error: FileError::WriteFailed }
            }

            return Result<i32, FileError>::Ok { value: written as i32 }
        }

        static fn append_string(path: cstr, contents: cstr) -> Result<i32, FileError> {
            var file = c_fopen(path, "a")
            if (file == null) {
                return Result<i32, FileError>::Error { error: FileError::OpenFailed }
            }

            var length = contents.length() as i64
            var written = c_fwrite(contents as *void, 1 as i64, length, file)
            c_fclose(file)

            if (written != length) {
                return Result<i32, FileError>::Error { error: FileError::WriteFailed }
            }

            return Result<i32, FileError>::Ok { value: written as i32 }
        }

        static fn write_bytes(path: cstr, data: *void, length: i64) -> Result<i32, FileError> {
            var file = c_fopen(path, "wb")
            if (file == null) {
                return Result<i32, FileError>::Error { error: FileError::OpenFailed }
            }

            var written = c_fwrite(data, 1 as i64, length, file)
            c_fclose(file)

            if (written != length) {
                return Result<i32, FileError>::Error { error: FileError::WriteFailed }
            }

            return Result<i32, FileError>::Ok { value: written as i32 }
        }

        static fn read_bytes(path: cstr) -> Result<Bytes, FileError> {
            var file = c_fopen(path, "rb")
            if (file == null) {
                return Result<Bytes, FileError>::Error { error: FileError::OpenFailed }
            }

            c_fseek(file, 0, 2)
            var size = c_ftell(file)
            c_fseek(file, 0, 0)

            if (size <= (0 as i64)) {
                c_fclose(file)
                return Result<Bytes, FileError>::Ok { value: Bytes() }
            }

            var buffer = Memory::allocate_bytes(size) as *mut u8
            var read = c_fread(buffer as *mut void, 1 as i64, size, file)
            c_fclose(file)

            if (read != size) {
                Memory::free(buffer as *mut void)
                return Result<Bytes, FileError>::Error { error: FileError::ReadFailed }
            }

            return Result<Bytes, FileError>::Ok { value: Bytes { ptr: buffer, length: size } }
        }

        static fn create_directory(path: cstr) -> Result<i32, FileError> {
            var result = c_mkdir(path, 493)
            if (result != 0) {
                return Result<i32, FileError>::Error { error: FileError::CreateDirectoryFailed }
            }

            return Result<i32, FileError>::Ok { value: 0 }
        }

        static fn create_directories(path: cstr) -> Result<i32, FileError> {
            var length = c_strlen(path) as i32
            var mut prefix = String()
            var mut index = 0

            if (length > 0 && path[0] == '/') {
                prefix.push('/')
                index = 1
            }

            while (index <= length) {
                if (index == length || path[index] == '/') {
                    if (prefix.length() > 0 && !prefix.equals("/") && !File::exists(prefix.as_cstr())) {
                        var result = c_mkdir(prefix.as_cstr(), 493)
                        if (result != 0) {
                            return Result<i32, FileError>::Error { error: FileError::CreateDirectoryFailed }
                        }
                    }
                }

                if (index < length) {
                    prefix.push(path[index])
                }
                index = index + 1
            }

            return Result<i32, FileError>::Ok { value: 0 }
        }

        static fn exists(path: cstr) -> boolean {
            return c_access(path, AccessMode::Exists.value) == 0
        }

        static fn remove(path: cstr) -> Result<i32, FileError> {
            var result = c_remove(path)
            if (result != 0) {
                return Result<i32, FileError>::Error { error: FileError::RemoveFailed }
            }
            return Result<i32, FileError>::Ok { value: 0 }
        }

        static fn copy_recursive(source: cstr, destination: cstr) -> Result<i32, FileError> {
            var created = File::create_directory(destination)
            if (created.is_error()) {
                return Result<i32, FileError>::Error { error: FileError::CopyFailed }
            }

            var listed = Directory::list(source)
            if (listed.is_error()) {
                return Result<i32, FileError>::Error { error: FileError::CopyFailed }
            }

            var entries = listed.unwrap()
            for (var mut entry_index: i32 = 0; entry_index < entries.size(); entry_index = entry_index + 1) {
                var entry = entries.get(entry_index).unwrap()
                var source_path = Path::join(source, entry->name.as_cstr())
                var destination_path = Path::join(destination, entry->name.as_cstr())

                if (entry->is_directory) {
                    var copied = File::copy_recursive(source_path.as_cstr(), destination_path.as_cstr())
                    if (copied.is_error()) {
                        return Result<i32, FileError>::Error { error: FileError::CopyFailed }
                    }
                } else {
                    var read = File::read_bytes(source_path.as_cstr())
                    if (read.is_error()) {
                        return Result<i32, FileError>::Error { error: FileError::CopyFailed }
                    }

                    var bytes = read.unwrap()
                    var written = File::write_bytes(destination_path.as_cstr(), bytes.as_ptr(), bytes.length)
                    if (written.is_error()) {
                        return Result<i32, FileError>::Error { error: FileError::CopyFailed }
                    }
                }
            }

            return Result<i32, FileError>::Ok { value: 0 }
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
            var mut entry = c_readdir(directory)

            while (entry != null) {
                var name = Directory::entry_name(entry)
                if (!Directory::is_dot_entry(name)) {
                    entries.push(DirectoryEntry { name: String(name), is_directory: Directory::entry_is_directory(entry) })
                }
                entry = c_readdir(directory)
            }

            c_closedir(directory)

            return Result<Vector<DirectoryEntry>, FileError>::Ok { value: entries }
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
            var buffer = Memory::allocate_bytes(4096) as *mut char
            var resolved = c_realpath(path, buffer)
            if (resolved == null) {
                Memory::free(buffer as *mut void)
                return Result<String, FileError>::Error { error: FileError::ResolvePathFailed }
            }

            var result = String(resolved)
            Memory::free(buffer as *mut void)

            return Result<String, FileError>::Ok { value: result }
        }

        static fn basename(path: cstr) -> String {
            var length = path.length()
            for (var mut index: i32 = length - 1; index >= 0; index = index - 1) {
                if (path[index] == '/') {
                    return String((&path[index + 1]) as cstr)
                }
            }
            return String(path)
        }

        static fn dirname(path: cstr) -> String {
            var length = path.length()
            for (var mut separator_index: i32 = length - 1; separator_index >= 0;
                 separator_index = separator_index - 1) {
                if (path[separator_index] == '/') {
                    if (separator_index == 0) {
                        return String("/")
                    }

                    var mut result = String()
                    for (var mut index: i32 = 0; index < separator_index; index = index + 1) {
                        result.push(path[index])
                    }

                    return String(result.as_cstr())
                }
            }
            return String(".")
        }
}
