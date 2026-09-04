import std.ffi.zep_errno
import std.text.string.String

public struct SocketError {
    public:
        var code: i32

        static fn last() -> SocketError {
            var ptr = zep_errno()
            if (ptr == null) {
                return SocketError { code: 0 }
            }

            return SocketError { code: *ptr }
        }

        fn to_string() -> String {
            var mut message = String("socket error ")
            message.append(String::from(code).as_cstr())
            return String(message.as_cstr())
        }
}
