import std.core.option.Option
import std.core.result.Result
import std.memory.Memory
import std.io.panic.panic

public enum IndexError {
    OutOfBounds { index: i64, size: i64 }
}

public struct Vector<T> {
    private:
        var ptr: *mut T
        var length: i64
        var capacity: i64

        fn ensure_capacity(required_capacity: i64) mut -> void {
            if (required_capacity < 0) {
                panic("vector capacity overflow")
            }
            if (required_capacity <= capacity) {
                return
            }

            var mut next_capacity: i64 = 4
            if (capacity != 0 && capacity <= (9223372036854775807 as i64) / 2) {
                next_capacity = capacity * 2
            }
            while (next_capacity < required_capacity) {
                if (next_capacity > (9223372036854775807 as i64) / 2) {
                    next_capacity = required_capacity
                } else {
                    next_capacity = next_capacity * 2
                }
            }

            self->reallocate(next_capacity)
        }

        fn reallocate(next_capacity: i64) mut -> void {
            var next_ptr = Memory::allocate_many<T>(next_capacity as i64)

            if (ptr != null) {
                for (var mut index: i64 = 0; index < length; index = index + 1) {
                    new (&mut next_ptr[index]) Memory::move(ptr[index])
                    ptr[index].~T()
                }

                Memory::free(ptr as *mut void)
            }

            ptr = next_ptr
            capacity = next_capacity
        }

    public:
        fn Vector(source: &Vector<T>) -> Vector<T> {
            var storage = Memory::allocate_many<T>(source.length as i64)
            for (var mut index: i64 = 0; index < source.length; index = index + 1) {
                new (&mut storage[index]) source.ptr[index]
            }
            return Vector<T> { ptr: storage, length: source.length, capacity: source.length }
        }

        fn Vector(source: move Vector<T>) -> Vector<T> {
            var storage = source.ptr
            var length = source.length
            var capacity = source.capacity
            source.ptr = null
            source.length = 0
            source.capacity = 0
            return Vector<T> { ptr: storage, length: length, capacity: capacity }
        }

        fn Vector() -> Vector<T> {
            return Vector<T> { ptr: null, length: 0, capacity: 0 }
        }

        fn Vector(initial_capacity: i64) -> Vector<T> {
            if (initial_capacity < 0) {
                panic("negative vector capacity")
            }
            if (initial_capacity == 0) {
                return Vector<T>()
            }

            return Vector<T> {
                ptr: Memory::allocate_many<T>(initial_capacity as i64),
                length: 0,
                capacity: initial_capacity
            }
        }

        fn push(value: T) mut -> void {
            self->ensure_capacity(length + 1)
            new (&mut ptr[length]) Memory::move(value)
            length = length + 1
        }

        fn pop() mut -> Option<T> {
            if (length == 0) {
                return Option<T>::None
            }

            length = length - 1
            var result = Option<T>::Some { value: Memory::move(ptr[length]) }
            ptr[length].~T()
            return Memory::move(result)
        }

        fn insert(index: i64, value: T) mut -> Result<i64, IndexError> {
            if (index < 0 || index > length) {
                return Result<i64, IndexError>::Error {
                    error: IndexError::OutOfBounds { index: index, size: length }
                }
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
            return Result<i64, IndexError>::Ok { value: index }
        }

        fn remove(index: i64) mut -> Option<T> {
            if (index < 0 || index >= length) {
                return Option<T>::None
            }

            var value = Memory::move(ptr[index])

            for (var mut current_index = index; current_index < length - 1;
                 current_index = current_index + 1) {
                ptr[current_index] = Memory::move(ptr[current_index + 1])
            }

            length = length - 1
            ptr[length].~T()
            return Option<T>::Some { value: Memory::move(value) }
        }

        fn get(index: i64) -> Option<*T> {
            if (index < 0 || index >= length) {
                return Option<*T>::None
            }

            return Option<*T>::Some { value: &ptr[index] }
        }

        fn get_mut(index: i64) mut -> Option<*mut T> {
            if (index < 0 || index >= length) {
                return Option<*mut T>::None
            }

            return Option<*mut T>::Some { value: &mut ptr[index] }
        }

        fn first() -> Option<*T> {
            return self->get(0)
        }

        fn last() -> Option<*T> {
            return self->get(length - 1)
        }

        fn set(index: i64, value: T) mut -> Option<T> {
            if (index < 0 || index >= length) {
                return Option<T>::None
            }

            var previous = Memory::move(ptr[index])
            ptr[index] = Memory::move(value)
            return Option<T>::Some { value: Memory::move(previous) }
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

        fn reserve(minimum_capacity: i64) mut -> void {
            self->ensure_capacity(minimum_capacity)
        }

        fn shrink_to_fit() mut -> void {
            if (length == capacity) {
                return
            }

            if (length == 0) {
                if (ptr != null) {
                    Memory::free(ptr as *mut void)
                }

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

        fn is_empty() -> boolean {
            return length == 0
        }

        fn clear() mut -> void {
            while (length > 0) {
                length = length - 1
                ptr[length].~T()
            }
        }

        fn ~Vector() -> void {
            self->clear()

            if (ptr != null) {
                Memory::free(ptr as *mut void)
                ptr = null
                capacity = 0
            }
        }
}
