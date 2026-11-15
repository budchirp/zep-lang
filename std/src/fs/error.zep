import std.io.error.IoError
import std.text.string.String
import std.memory.Memory

public enum FileError(error: IoError) {
    Open
    Read
    Write
    Seek
    Close
    CreateDirectory
    Remove
    ReadDirectory
    ResolvePath

    public:
        fn to_string() -> String {
            var mut message = String(when (*self) {
                FileError::Open -> "failed to open file",
                FileError::Read -> "failed to read file",
                FileError::Write -> "failed to write file",
                FileError::Seek -> "failed to seek file",
                FileError::Close -> "failed to close file",
                FileError::CreateDirectory -> "failed to create directory",
                FileError::Remove -> "failed to remove file",
                FileError::ReadDirectory -> "failed to read directory",
                FileError::ResolvePath -> "failed to resolve path",
            })

            message.append(": error ")
            message.append(String::from(error.value))

            return Memory::move(message)
        }
}
