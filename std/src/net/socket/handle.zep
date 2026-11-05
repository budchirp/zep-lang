import std.ffi.c_close
import std.ffi.FfiFileDescriptor
import std.core.result.Result
import std.core.unit.Unit
import std.net.socket.error.SocketError

public struct SocketHandle {
    public:
        var file_descriptor: i32

        fn SocketHandle(source: &SocketHandle) -> SocketHandle = delete

        fn SocketHandle(source: move SocketHandle) -> SocketHandle {
            var descriptor = source.file_descriptor
            source.file_descriptor = FfiFileDescriptor::Invalid.value

            return SocketHandle { file_descriptor: descriptor }
        }

        fn close() mut -> Result<Unit, SocketError> {
            if (file_descriptor == FfiFileDescriptor::Invalid.value) {
                return Result<Unit, SocketError>::Ok { value: Unit {} }
            }

            var descriptor = file_descriptor
            file_descriptor = FfiFileDescriptor::Invalid.value

            if (c_close(descriptor) != 0) {
                return Result<Unit, SocketError>::Error { error: SocketError::last() }
            }

            return Result<Unit, SocketError>::Ok { value: Unit {} }
        }

        fn ~SocketHandle() -> void {
            self->close()
        }
}
