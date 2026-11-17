import std.ffi.c_access
import std.ffi.c_realpath
import std.core.box.Box
import std.fs.error.FileError
import std.fs.error.FileOperation
import std.io.error.IoError
import std.ffi.zep_errno
import std.memory.Memory
import std.text.string.String
import std.text.view.StringView

public struct Path {
    public:
        static fn exists(path: cstr) -> boolean {
            return c_access(path, 0) == 0
        }

        static fn join(left: cstr, right: cstr) -> String {
            if (right.starts_with("/") || left.is_empty()) {
                return String(right)
            }

            var mut result = String(left)

            if (!right.is_empty()) {
                if (left[left.length() - 1] != '/') {
                    result.push('/')
                }

                result.append(right)
            }

            return Memory::move(result)
        }

        @throws(FileError)
        static fn absolute(path: cstr) -> String {
            var storage = Box::from_raw(c_realpath(path, null) as *mut char)

            return when (storage.get()) {
                null -> throw FileError { operation: FileOperation::ResolvePath,
                    error: IoError { value: *zep_errno() } },
                else -> String(storage.get() as cstr),
            }
        }

        static fn basename(path: cstr) -> String {
            for (var mut index = path.length() - 1; index >= 0; index = index - 1) {
                if (path[index] == '/') {
                    return String((&path[index + 1]) as cstr)
                }
            }

            return String(path)
        }

        static fn dirname(path: cstr) -> String {
            for (var mut index = path.length() - 1; index >= 0; index = index - 1) {
                if (path[index] == '/') {
                    return when (index) {
                        0 -> String("/"),
                        else -> String(StringView(path, index)),
                    }
                }
            }

            return String(".")
        }
}
