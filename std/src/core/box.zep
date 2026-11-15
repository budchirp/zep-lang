import std.memory.Memory

public struct Box<T> {
    private:
        var ptr: *mut T

    public:
        fn Box(source: &Box<T>) -> Box<T> = delete

        fn Box(source: move Box<T>) -> Box<T> {
            var storage = source.ptr
            source.ptr = null

            return Box { ptr: storage }
        }

        fn Box(value: T) -> Box<T> {
            var storage: *mut T = Memory::allocate()
            new (storage) Memory::move(value)

            return Box { ptr: storage }
        }

        static fn from_raw(ptr: *mut T) -> Box<T> {
            return Box { ptr: ptr }
        }

        fn into_raw() move -> *mut T {
            var storage = ptr
            ptr = null

            return storage
        }

        fn get() -> *T {
            return ptr as *T
        }

        fn get_mut() mut -> *mut T {
            return ptr
        }

        fn replace(value: T) mut -> T {
            var previous = Memory::move(ptr[0])
            ptr[0] = Memory::move(value)

            return Memory::move(previous)
        }

        fn into_inner() move -> T {
            var value = Memory::move(ptr[0])
            ptr[0].~T()
            Memory::free(ptr as *mut void)
            ptr = null

            return Memory::move(value)
        }

        fn ~Box() -> void {
            if (ptr != null) {
                ptr[0].~T()
                Memory::free(ptr as *mut void)
            }
        }
}
