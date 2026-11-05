import std.ffi.c_accept
import std.memory.Memory
import std.ffi.c_bind
import std.ffi.c_getsockname
import std.ffi.c_listen
import std.ffi.c_setsockopt
import std.ffi.c_socket
import std.ffi.zep_errno
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
import std.net.socket.handle.SocketHandle

public struct TcpListener : SocketHandle {
    public:
        fn TcpListener(source: &TcpListener) -> TcpListener = delete

        fn TcpListener(source: move TcpListener) -> TcpListener = default

        static fn bind(address: Ipv4Address, port: u16) -> Result<TcpListener, SocketError> {
            var mut listener = TcpListener {
                file_descriptor: c_socket(FfiSocketDomain::Ipv4.value, FfiSocketKind::Stream.value, 0)
            }

            if (listener.file_descriptor < 0) {
                return Result<TcpListener, SocketError>::Error { error: SocketError::last() }
            }

            var mut enabled: i32 = 1
            var reuse_result = c_setsockopt(listener.file_descriptor, FfiSocketLevel::Socket.value,
                                            FfiSocketOption::reuse_address(),
                                            &mut enabled, #sizeof(i32) as u32)

            if (reuse_result < 0) {
                return Result<TcpListener, SocketError>::Error { error: SocketError::last() }
            }

            var mut socket_address = SocketAddress::ipv4(address, port)
            var bind_result = c_bind(listener.file_descriptor, socket_address.as_ptr(),
                                     FfiSocketAddressLength::Ipv4.value)
            if (bind_result < 0) {
                return Result<TcpListener, SocketError>::Error { error: SocketError::last() }
            }

            var listen_result = c_listen(listener.file_descriptor, 16)
            if (listen_result < 0) {
                return Result<TcpListener, SocketError>::Error { error: SocketError::last() }
            }

            return Result<TcpListener, SocketError>::Ok {
                value: Memory::move(listener)
            }
        }

        fn accept() mut -> Result<TcpStream, SocketError> {
            var mut socket_address = SocketAddress()
            var mut length = FfiSocketAddressLength::Ipv4.value
            var mut stream_file_descriptor = c_accept(file_descriptor, socket_address.as_mut_ptr(), &mut length)
            while (stream_file_descriptor < 0 && *zep_errno() == 4) {
                stream_file_descriptor = c_accept(file_descriptor, socket_address.as_mut_ptr(), &mut length)
            }

            if (stream_file_descriptor < 0) {
                return Result<TcpStream, SocketError>::Error { error: SocketError::last() }
            }

            return Result<TcpStream, SocketError>::Ok { value: TcpStream(stream_file_descriptor) }
        }

        fn local_port() -> Result<u16, SocketError> {
            var mut socket_address = SocketAddress()
            var mut length = FfiSocketAddressLength::Ipv4.value
            var result = c_getsockname(file_descriptor, socket_address.as_mut_ptr(), &mut length)
            if (result < 0) {
                return Result<u16, SocketError>::Error { error: SocketError::last() }
            }

            return Result<u16, SocketError>::Ok { value: socket_address.port() }
        }

}
