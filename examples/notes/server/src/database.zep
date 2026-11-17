import std.collections.Vector
import std.core.Option
import std.memory.Memory
import std.text.String

public struct Note {
    public:
        var id: i64
        var body: String
}

public struct Database {
    private:
        var notes: Vector<Note>
        var next_id: i64

        fn find(id: i64) -> Option<i64> {
            for (var mut index: i64 = 0; index < notes.length(); index = index + 1) {
                if (notes.get(index).unwrap()->id == id) {
                    return Option::Some { value: index }
                }
            }

            return Option::None
        }

    public:
        fn Database() -> Database {
            return Database { notes: Vector(), next_id: 1 }
        }

        fn create(body: &String) mut -> i64 {
            var id = next_id
            next_id = next_id + 1
            notes.push(Note { id: id, body: body })

            return id
        }

        fn get(id: i64) -> Option<*Note> {
            return when (self->find(id)) {
                Option::Some { value } -> notes.get(value),
                Option::None -> Option::None,
            }
        }

        fn update(id: i64, body: &String) mut -> boolean {
            return when (self->find(id)) {
                Option::Some { value } -> {
                    notes.get_mut(value).unwrap()->body = body

                    true
                },
                Option::None -> false,
            }
        }

        fn remove(id: i64) mut -> boolean {
            return when (self->find(id)) {
                Option::Some { value } -> notes.remove(value).is_some(),
                Option::None -> false,
            }
        }

        fn list() -> String {
            var mut text = String()

            for (var mut index: i64 = 0; index < notes.length(); index = index + 1) {
                var note = notes.get(index).unwrap()
                text.append(String::from(note->id))
                text.append("\t")
                text.append(note->body)
                text.append("\n")
            }

            return Memory::move(text)
        }
}
