import std.core.option.Option
import std.core.error.Error
import std.memory.Memory

public enum CapacityFailure : i32 {
    Negative = 0
    Overflow = 1
}

public struct CapacityError : Error {
    public:
        var reason: CapacityFailure
}

public struct IndexError : Error {
    public:
        var index: i64
        var length: i64
}

public struct Vector<T> {
    private:
        var ptr: *mut T
        var length: i64
        var capacity: i64

        fn destroy_tail(count: i64) mut -> void {
            while (length > count) {
                length = length - 1
                ptr[length].~T()
            }
        }

        fn ensure_capacity(required_capacity: i64) mut -> void {
            if (required_capacity >= 0 && required_capacity <= capacity) {
                return
            }

            self->reallocate(Memory::grow_capacity(capacity, required_capacity))
        }

        fn reallocate(next_capacity: i64) mut -> void {
            var next_ptr: *mut T = Memory::allocate_many(next_capacity)

            for (var mut index: i64 = 0; index < length; index = index + 1) {
                new (&mut next_ptr[index]) Memory::move(ptr[index])
                ptr[index].~T()
            }

            Memory::free(ptr as *mut void)

            ptr = next_ptr
            capacity = next_capacity
        }

    public:
        fn Vector(source: &Vector<T>) -> Vector<T> {
            var storage: *mut T = Memory::allocate_many(source.length)

            for (var mut index: i64 = 0; index < source.length; index = index + 1) {
                new (&mut storage[index]) source.ptr[index]
            }

            return Vector { ptr: storage, length: source.length, capacity: source.length }
        }

        fn Vector(source: move Vector<T>) -> Vector<T> {
            var storage = source.ptr
            var length = source.length
            var capacity = source.capacity

            source.ptr = null
            source.length = 0
            source.capacity = 0

            return Vector { ptr: storage, length: length, capacity: capacity }
        }

        fn Vector() -> Vector<T> {
            return Vector { ptr: null, length: 0, capacity: 0 }
        }

        @throws(CapacityError)
        static fn with_capacity(capacity: i64) -> Vector<T> {
            var mut value: Vector<T> = Vector()
            try value.reserve(capacity)

            return Memory::move(value)
        }

        fn push(value: T) mut -> void {
            self->ensure_capacity(length + 1)
            new (&mut ptr[length]) Memory::move(value)
            length = length + 1
        }

        fn pop() mut -> Option<T> {
            if (length == 0) {
                return Option::None
            }

            length = length - 1
            var result: Option<T> = Option::Some { value: Memory::move(ptr[length]) }
            ptr[length].~T()

            return Memory::move(result)
        }

        @throws(IndexError)
        fn insert(index: i64, value: T) mut -> i64 {
            if (index < 0 || index > length) {
                throw IndexError { index: index, length: length }
            }

            self->ensure_capacity(length + 1)

            var mut current_index = length
            while (current_index > index) {
                if (current_index < length) {
                    ptr[current_index].~T()
                }
                new (&mut ptr[current_index]) Memory::move(ptr[current_index - 1])
                current_index = current_index - 1
            }

            if (index < length) {
                ptr[index].~T()
            }
            new (&mut ptr[index]) Memory::move(value)
            length = length + 1
            return index
        }

        fn remove(index: i64) mut -> Option<T> {
            if (index < 0 || index >= length) {
                return Option::None
            }

            var value = Memory::move(ptr[index])

            for (var mut current_index = index; current_index < length - 1;
                 current_index = current_index + 1) {
                ptr[current_index] = Memory::move(ptr[current_index + 1])
            }

            length = length - 1
            ptr[length].~T()
            return Option::Some { value: Memory::move(value) }
        }

        fn get(index: i64) -> Option<*T> {
            if (index < 0 || index >= length) {
                return Option::None
            }

            return Option::Some { value: &ptr[index] }
        }

        fn get_mut(index: i64) mut -> Option<*mut T> {
            if (index < 0 || index >= length) {
                return Option::None
            }

            return Option::Some { value: &mut ptr[index] }
        }

        fn first() -> Option<*T> {
            return self->get(0)
        }

        fn last() -> Option<*T> {
            return self->get(length - 1)
        }

        fn set(index: i64, value: T) mut -> Option<T> {
            if (index < 0 || index >= length) {
                return Option::None
            }

            var previous = Memory::move(ptr[index])
            ptr[index] = Memory::move(value)
            return Option::Some { value: Memory::move(previous) }
        }

        fn swap(left_index: i64, right_index: i64) mut -> boolean {
            if (left_index < 0 || left_index >= length || right_index < 0 ||
                right_index >= length) {
                return false
            }

            var temporary = Memory::move(ptr[left_index])
            ptr[left_index] = Memory::move(ptr[right_index])
            ptr[right_index] = Memory::move(temporary)
            return true
        }

        @throws(CapacityError)
        fn reserve(minimum_capacity: i64) mut -> void {
            if (minimum_capacity < 0) {
                throw CapacityError { reason: CapacityFailure::Negative }
            }

            if (#sizeof(T) > 0 && minimum_capacity > (9223372036854775807 as i64) / #sizeof(T)) {
                throw CapacityError { reason: CapacityFailure::Overflow }
            }

            self->ensure_capacity(minimum_capacity)
        }

        fn shrink_to_fit() mut -> void {
            if (length == capacity) {
                return
            }

            if (length == 0) {
                Memory::free(ptr as *mut void)

                ptr = null
                capacity = 0
                return
            }

            self->reallocate(length)
        }

        fn length() -> i64 {
            return length
        }

        fn capacity() -> i64 {
            return capacity
        }

        fn as_ptr() -> *T {
            return ptr as *T
        }

        fn as_mut_ptr() mut -> *mut T {
            return ptr
        }

        fn is_empty() -> boolean {
            return length == 0
        }

        fn clear() mut -> void {
            self->destroy_tail(0)
        }

        @throws(IndexError)
        fn truncate(count: i64) mut -> void {
            if (count < 0) {
                throw IndexError { index: count, length: length }
            }

            self->destroy_tail(count)
        }

        @throws(CapacityError)
        fn resize(count: i64, value: T) mut -> void {
            try self->reserve(count)
            self->destroy_tail(count)

            while (length < count) {
                new (&mut ptr[length]) value
                length = length + 1
            }
        }

        fn ~Vector() -> void {
            self->clear()
            Memory::free(ptr as *mut void)
        }
}
