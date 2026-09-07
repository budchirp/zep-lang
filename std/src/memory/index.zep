import std.ffi.c_malloc
import std.ffi.c_free

public struct Memory {
    public:
        static fn allocate<T>() -> *mut T {
            return c_malloc(#sizeof(T)) as *mut T
        }

        static fn allocate_bytes(size: i64) -> *mut void {
            return c_malloc(size)
        }

        static fn allocate_many<T>(count: i64) -> *mut T {
            return c_malloc(count * #sizeof(T)) as *mut T
        }

        static fn free(ptr: *mut void) -> void {
            if (ptr != null) {
                c_free(ptr)
            }
        }
}
