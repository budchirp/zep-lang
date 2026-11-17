import std.net.http.Method
import std.testing.Test
import std.text.String

test "commands validate arity and build resource paths" {
    var arguments = ["notes", "update", "7", "hello"]
    var command = Command::parse(4, (&arguments) as *cstr).unwrap()

    Test::expect(command.method == Method::Put)
    Test::expect(command.path.equals("/notes/7") && command.body.equals("hello"))
    Test::expect(Command::parse(3, (&arguments) as *cstr).is_none())
    Test::expect(Command::parse(1, (&arguments) as *cstr).is_none())
}

test "commands own their text and list uses the collection path" {
    var mut text = String("owned")
    var arguments = ["notes", "create", text.as_cstr()]
    var command = Command::parse(3, (&arguments) as *cstr).unwrap()
    text.clear()

    Test::expect(command.method == Method::Post && command.body.equals("owned"))

    var list = ["notes", "list"]
    var listing = Command::parse(2, (&list) as *cstr).unwrap()

    Test::expect(listing.method == Method::Get && listing.path.equals("/notes"))
}
