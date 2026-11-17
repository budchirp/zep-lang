import std.io.error.IoError
import std.core.error.Error
import std.text.string.String
import std.memory.Memory

public enum FileOperation : i32 {
    Open = 0
    Read = 1
    Write = 2
    Seek = 3
    Close = 4
    CreateDirectory = 5
    Remove = 6
    ReadDirectory = 7
    ResolvePath = 8
}

public struct FileError : Error {
    public:
        var operation: FileOperation
        var error: IoError

        fn to_string() -> String {
            var mut message = String(when (operation) {
                FileOperation::Open -> "failed to open file",
                FileOperation::Read -> "failed to read file",
                FileOperation::Write -> "failed to write file",
                FileOperation::Seek -> "failed to seek file",
                FileOperation::Close -> "failed to close file",
                FileOperation::CreateDirectory -> "failed to create directory",
                FileOperation::Remove -> "failed to remove file",
                FileOperation::ReadDirectory -> "failed to read directory",
                FileOperation::ResolvePath -> "failed to resolve path",
            })

            message.append(": error ")
            message.append(String::from(error.value))

            return Memory::move(message)
        }
}
