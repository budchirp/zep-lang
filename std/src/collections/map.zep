import std.core.option.Option
import std.memory.Memory
import std.core.pair.Pair
import std.collections.vector.Vector

public struct Map<K, V> {
    private:
        var entries: Vector<Pair<K, V>>
        var equality_function: (*K, *K) -> boolean

        fn find_index(key: *K) -> Option<i64> {
            for (var mut index: i64 = 0; index < entries.length(); index = index + 1) {
                var candidate = entries.get(index).unwrap()
                var compare = equality_function
                if (compare(&candidate->first, key)) {
                    return Option<i64>::Some { value: index }
                }
            }

            return Option<i64>::None
        }

    public:
        fn Map(equality_function: (*K, *K) -> boolean) -> Map<K, V> {
            return Map<K, V> {
                entries: Vector<Pair<K, V>>(),
                equality_function: equality_function
            }
        }

        fn set(key: K, value: V) mut -> Option<V> {
            var index = self->find_index(&key)
            if (index.is_some()) {
                var entry = entries.get_mut(index.unwrap()).unwrap()
                var previous = Memory::move(entry->second)
                entry->second = Memory::move(value)
                return Option<V>::Some { value: Memory::move(previous) }
            }

            entries.push(Pair<K, V> { first: Memory::move(key), second: Memory::move(value) })
            return Option<V>::None
        }

        fn get(key: *K) -> Option<*V> {
            var index = self->find_index(key)
            if (index.is_none()) {
                return Option<*V>::None
            }

            var entry = entries.get(index.unwrap()).unwrap()
            return Option<*V>::Some { value: &entry->second }
        }

        fn get_mut(key: *K) mut -> Option<*mut V> {
            var index = self->find_index(key)
            if (index.is_none()) {
                return Option<*mut V>::None
            }

            var entry = entries.get_mut(index.unwrap()).unwrap()
            return Option<*mut V>::Some { value: &mut entry->second }
        }

        fn contains_key(key: *K) -> boolean {
            return self->find_index(key).is_some()
        }

        fn remove(key: *K) mut -> Option<V> {
            var index = self->find_index(key)
            if (index.is_none()) {
                return Option<V>::None
            }

            var removed = entries.remove(index.unwrap())
            var entry = Memory::move(removed).unwrap()
            return Option<V>::Some { value: Memory::move(entry.second) }
        }

        fn length() -> i64 {
            return entries.length()
        }

        fn is_empty() -> boolean {
            return entries.is_empty()
        }

        fn clear() mut -> void {
            entries.clear()
        }
}
