import std.core.error.Error

public enum IoCode : i32 {
    NotFound = 2
    Interrupted = 4
    Failed = 5
    Closed = 9
    PermissionDenied = 13
    Exists = 17
    InvalidInput = 22
    NotSeekable = 29
    BrokenPipe = 32
}

public struct IoError : Error {
    public:
        var value: i32
}
