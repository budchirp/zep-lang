import std.core.option.Option
import std.memory.Memory
import std.core.pair.Pair
import std.collections.vector.Vector

public struct Map<K, V> {
    private:
        var entries: Vector<Pair<K, V>>
        var equality_function: (&K, &K) -> boolean

        fn find_index(key: &K) -> Option<i64> {
            for (var mut index: i64 = 0; index < entries.length(); index = index + 1) {
                var candidate = entries.get(index).unwrap()
                if (equality_function(candidate->first, key)) {
                    return Option::Some { value: index }
                }
            }

            return Option::None
        }

    public:
        fn Map(equality_function: (&K, &K) -> boolean) -> Map<K, V> {
            return Map {
                entries: Vector(),
                equality_function: equality_function
            }
        }

        fn set(key: K, value: V) mut -> Option<V> {
            return when (self->find_index(key)) {
                Option::Some { value: index } -> {
                    var entry = entries.get_mut(index).unwrap()
                    var previous = Memory::move(entry->second)
                    entry->second = Memory::move(value)

                    Option::Some { value: Memory::move(previous) }
                },
                Option::None -> {
                    entries.push(Pair { first: Memory::move(key), second: Memory::move(value) })

                    Option::None
                },
            }
        }

        fn get(key: &K) -> Option<*V> {
            return when (self->find_index(key)) {
                Option::None -> Option::None,
                Option::Some { value } -> Option::Some { value: &entries.get(value).unwrap()->second },
            }
        }

        fn get_mut(key: &K) mut -> Option<*mut V> {
            return when (self->find_index(key)) {
                Option::None -> Option::None,
                Option::Some { value } -> Option::Some { value: &mut entries.get_mut(value).unwrap()->second },
            }
        }

        fn contains_key(key: &K) -> boolean {
            return self->find_index(key).is_some()
        }

        fn remove(key: &K) mut -> Option<V> {
            return when (self->find_index(key)) {
                Option::None -> Option::None,
                Option::Some { value } -> {
                    var entry = entries.remove(value).unwrap()

                    Option::Some { value: Memory::move(entry.second) }
                },
            }
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
