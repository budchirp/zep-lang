import std.core.option.Option
import std.collections.vector.Vector

public struct Map<K, V> {
    private:
        var keys: Vector<K>
        var values: Vector<V>
        var equality_function: (*K, *K) -> boolean

        fn find_index(key: *K) -> Option<i32> {
            for (var mut index: i32 = 0; index < keys.size(); index = index + 1) {
                var candidate = keys.get(index).unwrap()
                var compare = equality_function
                if (compare(candidate, key)) {
                    return Option<i32>::Some { value: index }
                }
            }

            return Option<i32>::None
        }

    public:
        fn Map(equality_function: (*K, *K) -> boolean) -> Map<K, V> {
            return Map<K, V> {
                keys: Vector<K>(),
                values: Vector<V>(),
                equality_function: equality_function
            }
        }

        fn set(key: K, value: V) mut -> Option<V> {
            var index = self->find_index(&key)
            if (index.is_some()) {
                return values.set(index.unwrap(), value)
            }

            keys.push(key)
            values.push(value)
            return Option<V>::None
        }

        fn get(key: *K) -> Option<*V> {
            var index = self->find_index(key)
            if (index.is_none()) {
                return Option<*V>::None
            }

            return values.get(index.unwrap())
        }

        fn get_mut(key: *K) mut -> Option<*mut V> {
            var index = self->find_index(key)
            if (index.is_none()) {
                return Option<*mut V>::None
            }

            return values.get_mut(index.unwrap())
        }

        fn contains_key(key: *K) -> boolean {
            return self->find_index(key).is_some()
        }

        fn remove(key: *K) mut -> Option<V> {
            var index = self->find_index(key)
            if (index.is_none()) {
                return Option<V>::None
            }

            var resolved_index = index.unwrap()
            var removed_key = keys.remove(resolved_index)
            return values.remove(resolved_index)
        }

        fn size() -> i32 {
            return keys.size()
        }

        fn is_empty() -> boolean {
            return keys.is_empty()
        }

        fn clear() mut -> void {
            keys.clear()
            values.clear()
        }
}
