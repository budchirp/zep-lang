import std.core.option.Option
import std.core.result.Result
import std.memory.Memory

public enum IndexError {
    OutOfBounds { index: i32, size: i32 }
}

public struct Vector<T> {
    private:
        var ptr: *mut T
        var length: i32
        var capacity: i32

        fn ensure_capacity(required_capacity: i32) mut -> void {
            if (required_capacity <= capacity) {
                return
            }

            var mut next_capacity = if (capacity == 0) { 4 } else { capacity * 2 }
            while (next_capacity < required_capacity) {
                next_capacity = next_capacity * 2
            }

            self->reallocate(next_capacity)
        }

        fn reallocate(next_capacity: i32) mut -> void {
            var next_ptr = Memory::allocate_many<T>(next_capacity as i64)

            if (ptr != null) {
                for (var mut index: i32 = 0; index < length; index = index + 1) {
                    next_ptr[index] = ptr[index]
                }

                Memory::free(ptr as *mut void)
            }

            ptr = next_ptr
            capacity = next_capacity
        }

    public:
        fn Vector() -> Vector<T> {
            return Vector<T> { ptr: null, length: 0, capacity: 0 }
        }

        fn Vector(initial_capacity: i32) -> Vector<T> {
            if (initial_capacity <= 0) {
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
            ptr[length] = value
            length = length + 1
        }

        fn pop() mut -> Option<T> {
            if (length == 0) {
                return Option<T>::None
            }

            length = length - 1
            return Option<T>::Some { value: ptr[length] }
        }

        fn insert(index: i32, value: T) mut -> Result<i32, IndexError> {
            if (index < 0 || index > length) {
                return Result<i32, IndexError>::Error {
                    error: IndexError::OutOfBounds { index: index, size: length }
                }
            }

            self->ensure_capacity(length + 1)

            var mut current_index = length
            while (current_index > index) {
                ptr[current_index] = ptr[current_index - 1]
                current_index = current_index - 1
            }

            ptr[index] = value
            length = length + 1
            return Result<i32, IndexError>::Ok { value: index }
        }

        fn remove(index: i32) mut -> Option<T> {
            if (index < 0 || index >= length) {
                return Option<T>::None
            }

            var value = ptr[index]

            for (var mut current_index = index; current_index < length - 1;
                 current_index = current_index + 1) {
                ptr[current_index] = ptr[current_index + 1]
            }

            length = length - 1
            return Option<T>::Some { value: value }
        }

        fn get(index: i32) -> Option<*T> {
            if (index < 0 || index >= length) {
                return Option<*T>::None
            }

            return Option<*T>::Some { value: &ptr[index] }
        }

        fn get_mut(index: i32) mut -> Option<*mut T> {
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

        fn set(index: i32, value: T) mut -> Option<T> {
            if (index < 0 || index >= length) {
                return Option<T>::None
            }

            var previous = ptr[index]
            ptr[index] = value
            return Option<T>::Some { value: previous }
        }

        fn swap(left_index: i32, right_index: i32) mut -> boolean {
            if (left_index < 0 || left_index >= length || right_index < 0 ||
                right_index >= length) {
                return false
            }

            var temporary = ptr[left_index]
            ptr[left_index] = ptr[right_index]
            ptr[right_index] = temporary
            return true
        }

        fn reserve(minimum_capacity: i32) mut -> void {
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

        fn size() -> i32 {
            return length
        }

        fn capacity() -> i32 {
            return capacity
        }

        fn is_empty() -> boolean {
            return length == 0
        }

        fn clear() mut -> void {
            length = 0
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
