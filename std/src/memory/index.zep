import std.ffi.c_malloc
import std.ffi.c_free
import std.io.panic.panic

public struct Memory {
    public:
        static fn move<T>(value: &T) -> move T {
            return &value as move T
        }

        static fn allocate<T>() -> *mut T {
            return Memory::allocate_bytes(#sizeof(T)) as *mut T
        }

        static fn allocate_bytes(size: i64) -> *mut void {
            if (size < 0) {
                panic("allocation failed")
            }
            var mut allocation_size = size
            if (allocation_size == 0) {
                allocation_size = 1
            }
            var result = c_malloc(allocation_size)
            if (result == null) {
                panic("allocation failed")
            }
            return result
        }

        static fn allocate_many<T>(count: i64) -> *mut T {
            return Memory::allocate_bytes(Memory::allocation_size(count, #sizeof(T))) as *mut T
        }

        static fn allocation_size(count: i64, width: i64) -> i64 {
            if (count < 0 || width < 0) {
                panic("allocation failed")
            }

            if (width > 0 && count > (9223372036854775807 as i64) / width) {
                panic("allocation failed")
            }

            return count * width
        }

        static fn free(ptr: *mut void) -> void {
            c_free(ptr)
        }

}
