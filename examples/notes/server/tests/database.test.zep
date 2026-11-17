import std.testing.Test
import std.text.String

test "notes own text keep stable IDs and remove entries" {
    var mut database = Database()
    var mut text = String("first")
    var first = database.create(text)
    text.append(" changed")
    var second = database.create(String("second"))

    Test::expect(database.get(first).unwrap()->body.equals("first"))
    Test::expect(database.list().equals("1\tfirst\n2\tsecond\n"))
    Test::expect(database.update(first, text))
    Test::expect(database.get(first).unwrap()->body.equals("first changed"))
    Test::expect(database.remove(first))
    Test::expect(database.get(first).is_none())
    Test::expect(!database.update(first, text) && !database.remove(first))
    Test::expect(database.get(second).unwrap()->body.equals("second"))
    Test::expect(database.create(String("third")) == 3)
}
