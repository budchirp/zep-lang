import std.memory.Memory

public struct Box<T> {
    private:
        var ptr: *mut T

    public:
        fn Box(value: T) -> Box<T> {
            var ptr: *mut T = Memory::allocate<T>()
            ptr[0] = value

            return Box<T> { ptr: ptr }
        }

        fn get() -> *T {
            return ptr as *T
        }

        fn get_mut() mut -> *mut T {
            return ptr
        }

        fn replace(value: T) mut -> T {
            var previous = ptr[0]
            ptr[0] = value

            return previous
        }

        fn into_inner() mut -> T {
            var value = ptr[0]
            Memory::free(ptr as *mut void)
            ptr = null
            return value
        }

        fn ~Box() -> void {
            if (ptr != null) {
                Memory::free(ptr as *mut void)
                ptr = null
            }
        }
}
