import std.ffi.c_accept
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
import std.fs.file.File
import std.io.error.IoError
import std.io.error.IoCode
import std.memory.Memory
import std.net.socket.address.Ipv4Address
import std.net.socket.address.SocketAddress
import std.net.socket.stream.TcpStream

public struct TcpListener {
    private:
        var file: File

    public:
        @throws(IoError)
        static fn bind(address: Ipv4Address, port: u16) -> TcpListener {
            var listener = TcpListener {
                file: File::socket(c_socket(FfiSocketDomain::Ipv4.value, FfiSocketKind::Stream.value, 0))
            }

            if (listener.file.as_descriptor() < 0) {
                throw IoError { value: *zep_errno() }
            }

            var enabled: i32 = 1

            if (c_setsockopt(listener.file.as_descriptor(), FfiSocketLevel::Socket.value,
                             FfiSocketOption::reuse_address(), &enabled, #sizeof(i32) as u32) < 0) {
                throw IoError { value: *zep_errno() }
            }

            var socket_address = SocketAddress::ipv4(address, port)

            if (c_bind(listener.file.as_descriptor(), socket_address.as_ptr(),
                       FfiSocketAddressLength::Ipv4.value) < 0) {
                throw IoError { value: *zep_errno() }
            }

            return when {
                (c_listen(listener.file.as_descriptor(), 16) < 0) ->
                    throw IoError { value: *zep_errno() },
                else -> Memory::move(listener),
            }
        }

        @throws(IoError)
        fn accept() mut -> TcpStream {
            while (true) {
                var descriptor = c_accept(file.as_descriptor(), null, null)

                if (descriptor >= 0) {
                    return TcpStream(descriptor)
                }

                if (*zep_errno() != IoCode::Interrupted.value) {
                    throw IoError { value: *zep_errno() }
                }
            }
        }

        @throws(IoError)
        fn local_port() -> u16 {
            var mut address = SocketAddress()
            var mut length = FfiSocketAddressLength::Ipv4.value

            return when {
                (c_getsockname(file.as_descriptor(), address.as_mut_ptr(), &mut length) < 0) ->
                    throw IoError { value: *zep_errno() },
                else -> address.port(),
            }
        }
}
