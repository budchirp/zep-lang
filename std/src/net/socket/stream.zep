import std.ffi.c_connect
import std.ffi.c_socket
import std.ffi.zep_errno
import std.ffi.FfiSocketAddressLength
import std.ffi.FfiSocketDomain
import std.ffi.FfiSocketKind
import std.fs.file.File
import std.io.error.IoError
import std.memory.Memory
import std.net.socket.address.Ipv4Address
import std.net.socket.address.SocketAddress

public struct TcpStream {
    public:
        var file: File

        fn TcpStream(descriptor: i32) -> TcpStream {
            return TcpStream { file: File::socket(descriptor) }
        }

        @throws(IoError)
        static fn connect(address: Ipv4Address, port: u16) -> TcpStream {
            var stream = TcpStream(c_socket(FfiSocketDomain::Ipv4.value, FfiSocketKind::Stream.value, 0))

            if (stream.file.as_descriptor() < 0) {
                throw IoError { value: *zep_errno() }
            }

            var socket_address = SocketAddress::ipv4(address, port)

            return when {
                (c_connect(stream.file.as_descriptor(), socket_address.as_ptr(),
                           FfiSocketAddressLength::Ipv4.value) < 0) ->
                    throw IoError { value: *zep_errno() },
                else -> Memory::move(stream),
            }
        }
}
