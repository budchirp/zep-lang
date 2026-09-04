import std.ffi.c_accept
import std.ffi.c_bind
import std.ffi.c_close
import std.ffi.c_getsockname
import std.ffi.c_listen
import std.ffi.c_setsockopt
import std.ffi.c_socket
import std.ffi.FfiFileDescriptor
import std.ffi.FfiSocketAddressLength
import std.ffi.FfiSocketDomain
import std.ffi.FfiSocketKind
import std.ffi.FfiSocketLevel
import std.ffi.FfiSocketOption
import std.core.result.Result
import std.net.socket.address.Ipv4Address
import std.net.socket.address.SocketAddress
import std.net.socket.error.SocketError
import std.net.socket.stream.TcpStream

public struct TcpListener {
    private:
        var file_descriptor: i32

        static fn set_reuse_address(file_descriptor: i32) -> Result<i32, SocketError> {
            var mut enabled: i32 = 1
            var result = c_setsockopt(file_descriptor, FfiSocketLevel::Socket.value,
                                      FfiSocketOption::reuse_address(),
                                      &mut enabled, #sizeof(i32) as u32)

            if (result < 0) {
                return Result<i32, SocketError>::Error { error: SocketError::last() }
            }

            return Result<i32, SocketError>::Ok { value: result }
        }

    public:
        static fn bind(address: Ipv4Address, port: u16) -> Result<TcpListener, SocketError> {
            var file_descriptor = c_socket(FfiSocketDomain::Ipv4.value, FfiSocketKind::Stream.value, 0)
            if (file_descriptor < 0) {
                return Result<TcpListener, SocketError>::Error { error: SocketError::last() }
            }

            var reuse_result = TcpListener::set_reuse_address(file_descriptor)
            if (reuse_result.is_error()) {
                var error = SocketError::last()
                c_close(file_descriptor)

                return Result<TcpListener, SocketError>::Error { error: error }
            }

            var mut socket_address = SocketAddress::ipv4(address, port)
            var bind_result = c_bind(file_descriptor, socket_address.as_ptr(),
                                     FfiSocketAddressLength::Ipv4.value)
            if (bind_result < 0) {
                var error = SocketError::last()
                c_close(file_descriptor)

                return Result<TcpListener, SocketError>::Error { error: error }
            }

            var listen_result = c_listen(file_descriptor, 16)
            if (listen_result < 0) {
                var error = SocketError::last()
                c_close(file_descriptor)

                return Result<TcpListener, SocketError>::Error { error: error }
            }

            return Result<TcpListener, SocketError>::Ok {
                value: TcpListener { file_descriptor: file_descriptor }
            }
        }

        fn accept() mut -> Result<TcpStream, SocketError> {
            var mut socket_address = SocketAddress()
            var mut length = FfiSocketAddressLength::Ipv4.value
            var stream_file_descriptor = c_accept(file_descriptor, socket_address.as_mut_ptr(), &mut length)

            if (stream_file_descriptor < 0) {
                return Result<TcpStream, SocketError>::Error { error: SocketError::last() }
            }

            return Result<TcpStream, SocketError>::Ok { value: TcpStream(stream_file_descriptor) }
        }

        fn local_port() -> u16 {
            var mut socket_address = SocketAddress()
            var mut length = FfiSocketAddressLength::Ipv4.value
            var result = c_getsockname(file_descriptor, socket_address.as_mut_ptr(), &mut length)
            if (result < 0) {
                return 0 as u16
            }

            return socket_address.port()
        }

        fn close() mut -> void {
            if (file_descriptor != FfiFileDescriptor::Invalid.value) {
                c_close(file_descriptor)
                file_descriptor = FfiFileDescriptor::Invalid.value
            }
        }

        fn ~TcpListener() -> void {
            self->close()
        }
}
