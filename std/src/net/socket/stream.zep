import std.ffi.c_connect
import std.memory.Memory
import std.ffi.c_recv
import std.ffi.c_send
import std.ffi.c_socket
import std.ffi.zep_errno
import std.ffi.FfiSocketAddressLength
import std.ffi.FfiSocketDomain
import std.ffi.FfiSocketKind
import std.core.result.Result
import std.net.socket.address.Ipv4Address
import std.net.socket.address.SocketAddress
import std.net.socket.error.SocketError
import std.net.socket.handle.SocketHandle

public struct TcpStream : SocketHandle {
    public:
        fn TcpStream(source: &TcpStream) -> TcpStream = delete

        fn TcpStream(source: move TcpStream) -> TcpStream = default

        fn TcpStream(file_descriptor: i32) -> TcpStream {
            return TcpStream { file_descriptor: file_descriptor }
        }

        static fn connect(address: Ipv4Address, port: u16) -> Result<TcpStream, SocketError> {
            var mut stream = TcpStream(c_socket(FfiSocketDomain::Ipv4.value, FfiSocketKind::Stream.value, 0))

            if (stream.file_descriptor < 0) {
                return Result<TcpStream, SocketError>::Error { error: SocketError::last() }
            }

            var mut socket_address = SocketAddress::ipv4(address, port)
            var result = c_connect(stream.file_descriptor, socket_address.as_ptr(), FfiSocketAddressLength::Ipv4.value)
            if (result < 0) {
                return Result<TcpStream, SocketError>::Error { error: SocketError::last() }
            }

            return Result<TcpStream, SocketError>::Ok { value: Memory::move(stream) }
        }

        fn read(buffer: *mut void, size: i64) mut -> Result<i64, SocketError> {
            if (size < 0 || (buffer == null && size != 0)) {
                return Result<i64, SocketError>::Error { error: SocketError { code: 22 } }
            }
            var mut result = c_recv(file_descriptor, buffer, size, 0)
            while (result < 0 && *zep_errno() == 4) {
                result = c_recv(file_descriptor, buffer, size, 0)
            }
            if (result < 0 as i64) {
                return Result<i64, SocketError>::Error { error: SocketError::last() }
            }

            return Result<i64, SocketError>::Ok { value: result }
        }

        fn write(buffer: *void, size: i64) mut -> Result<i64, SocketError> {
            if (size < 0 || (buffer == null && size != 0)) {
                return Result<i64, SocketError>::Error { error: SocketError { code: 22 } }
            }
            var mut result = c_send(file_descriptor, buffer, size, TcpStream::send_flags())
            while (result < 0 && *zep_errno() == 4) {
                result = c_send(file_descriptor, buffer, size, TcpStream::send_flags())
            }
            if (result < 0 as i64) {
                return Result<i64, SocketError>::Error { error: SocketError::last() }
            }

            return Result<i64, SocketError>::Ok { value: result }
        }

        fn write_all(buffer: *void, size: i64) mut -> Result<i64, SocketError> {
            if (size < 0 || (buffer == null && size != 0)) {
                return Result<i64, SocketError>::Error { error: SocketError { code: 22 } }
            }
            var bytes = buffer as *u8
            var mut written: i64 = 0

            while (written < size) {
                var result = self->write(&bytes[written], size - written)

                if (result.is_error()) {
                    return result
                }

                var count = result.unwrap()

                if (count == 0) {
                    return Result<i64, SocketError>::Error { error: SocketError { code: 32 } }
                }

                written = written + count
            }

            return Result<i64, SocketError>::Ok { value: written }
        }

    private:
        @os("linux")
        static fn send_flags() -> i32 { return 16384 }

        @os("macos")
        static fn send_flags() -> i32 { return 0 }
}
