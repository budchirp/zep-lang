import std.ffi.c_close
import std.ffi.c_connect
import std.ffi.c_recv
import std.ffi.c_send
import std.ffi.c_socket
import std.ffi.FfiFileDescriptor
import std.ffi.FfiSocketAddressLength
import std.ffi.FfiSocketDomain
import std.ffi.FfiSocketKind
import std.core.result.Result
import std.net.socket.address.Ipv4Address
import std.net.socket.address.SocketAddress
import std.net.socket.error.SocketError

public struct TcpStream {
    private:
        var file_descriptor: i32

    public:
        fn TcpStream(file_descriptor: i32) -> TcpStream {
            return TcpStream { file_descriptor: file_descriptor }
        }

        static fn connect(address: Ipv4Address, port: u16) -> Result<TcpStream, SocketError> {
            var file_descriptor = c_socket(FfiSocketDomain::Ipv4.value, FfiSocketKind::Stream.value, 0)
            if (file_descriptor < 0) {
                return Result<TcpStream, SocketError>::Error { error: SocketError::last() }
            }

            var mut socket_address = SocketAddress::ipv4(address, port)
            var result = c_connect(file_descriptor, socket_address.as_ptr(), FfiSocketAddressLength::Ipv4.value)
            if (result < 0) {
                var error = SocketError::last()
                c_close(file_descriptor)

                return Result<TcpStream, SocketError>::Error { error: error }
            }

            return Result<TcpStream, SocketError>::Ok { value: TcpStream(file_descriptor) }
        }

        fn read(buffer: *mut void, size: i64) mut -> Result<i64, SocketError> {
            var result = c_recv(file_descriptor, buffer, size, 0)
            if (result < 0 as i64) {
                return Result<i64, SocketError>::Error { error: SocketError::last() }
            }

            return Result<i64, SocketError>::Ok { value: result }
        }

        fn write(buffer: *void, size: i64) mut -> Result<i64, SocketError> {
            var result = c_send(file_descriptor, buffer, size, 0)
            if (result < 0 as i64) {
                return Result<i64, SocketError>::Error { error: SocketError::last() }
            }

            return Result<i64, SocketError>::Ok { value: result }
        }

        fn write_all(buffer: *void, size: i64) mut -> Result<i64, SocketError> {
            var bytes = buffer as *u8
            var mut written: i64 = 0

            while (written < size) {
                var result = c_send(file_descriptor, &bytes[written], size - written, 0)
                if (result < 0 as i64) {
                    return Result<i64, SocketError>::Error { error: SocketError::last() }
                }

                if (result == 0 as i64) {
                    return Result<i64, SocketError>::Error { error: SocketError::last() }
                }

                written = written + result
            }

            return Result<i64, SocketError>::Ok { value: written }
        }

        fn close() mut -> void {
            if (file_descriptor != FfiFileDescriptor::Invalid.value) {
                c_close(file_descriptor)
                file_descriptor = FfiFileDescriptor::Invalid.value
            }
        }

        fn ~TcpStream() -> void {
            self->close()
        }
}
