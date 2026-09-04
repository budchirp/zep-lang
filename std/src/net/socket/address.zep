import std.ffi.c_htonl
import std.ffi.c_htons
import std.ffi.c_ntohs
import std.ffi.FfiSocketAddressLength
import std.ffi.FfiSocketDomain

var FAMILY_OFFSET_LINUX: i32 = 0
var PORT_OFFSET: i32 = 1
var ADDRESS_OFFSET: i32 = 1

public struct Ipv4Address {
    public:
        var value: u32

        static fn any() -> Ipv4Address {
            return Ipv4Address { value: 0 as u32 }
        }

        static fn localhost() -> Ipv4Address {
            return Ipv4Address { value: 2130706433 as u32 }
        }
}

public struct SocketAddress {
    public:
        var storage: u8[16]

        static fn ipv4(address: Ipv4Address, port: u16) -> SocketAddress {
            var mut result = SocketAddress()
            result.set_family()
            result.set_port(port)
            result.set_address(address)

            return result
        }

        fn SocketAddress() -> SocketAddress {
            var storage: u8[16]
            var mut result = SocketAddress { storage: storage }
            result.clear()

            return result
        }

        fn clear() mut -> void {
            for (var mut index: i32 = 0; index < FfiSocketAddressLength::Ipv4.value as i32;
                 index = index + 1) {
                storage[index] = 0 as u8
            }
        }

        @os("linux")
        fn set_family() mut -> void {
            var mut values = (&mut storage) as *mut u16
            values[FAMILY_OFFSET_LINUX] = FfiSocketDomain::Ipv4.value as u16
        }

        @os("macos")
        fn set_family() mut -> void {
            storage[0] = FfiSocketAddressLength::Ipv4.value as u8
            storage[1] = FfiSocketDomain::Ipv4.value as u8
        }

        fn set_port(port: u16) mut -> void {
            var mut values = (&mut storage) as *mut u16
            values[PORT_OFFSET] = c_htons(port)
        }

        fn set_address(address: Ipv4Address) mut -> void {
            var mut values = (&mut storage) as *mut u32
            values[ADDRESS_OFFSET] = c_htonl(address.value)
        }

        fn port() -> u16 {
            var values = (&storage) as *u16

            return c_ntohs(values[PORT_OFFSET])
        }

        fn as_mut_ptr() mut -> *mut void {
            return &mut storage
        }

        fn as_ptr() -> *void {
            return &storage
        }
}
