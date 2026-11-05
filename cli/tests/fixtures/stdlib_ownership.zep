import std.core.Box
import std.memory.Memory
import std.collections.Vector
import std.collections.Map
import std.text.String
import std.text.StringView
import std.io.FormatArguments
import std.io.format
import std.fs.File
import std.net.http.framing.MessageFraming
import std.net.http.framing.HttpParseError
import std.net.http.parser.RequestParser

fn same(left: *i32, right: *i32) -> boolean { return *left == *right }

public fn main() -> i32 {
    var mut raw: u8[3] = [97 as u8, 0 as u8, 98 as u8]
    var view = StringView((&raw) as cstr, 3)
    var mut text = String(&view)
    var copied = text
    text.append(&text)
    if (text.length() != 6 || copied.length() != 3 || copied.get(2).unwrap() != 'b') { return 1 }
    var moved = Memory::move(text)
    text.append("reused")
    if (moved.length() != 6 || !text.equals("reused")) { return 2 }

    var repeated = copied.repeat(3)
    if (repeated.length() != 9 || repeated.get(7).unwrap() != (0 as char)) { return 21 }
    var unchanged = copied.replace("", "x")
    if (!unchanged.equals(&copied)) { return 22 }
    var zero = StringView((&raw[1]) as cstr, 1)
    var replacement = StringView::from_cstr("!")
    var replaced = copied.replace(&zero, &replacement)
    if (!replaced.equals("a!b")) { return 23 }
    if (!String("MiXeD").to_lowercase().equals("mixed") ||
        !String("MiXeD").to_uppercase().equals("MIXED")) { return 24 }

    var mut owners = Vector<Box<i32>>()
    owners.push(Box<i32>(42))
    owners.push(Box<i32>(7))
    owners.swap(0, 1)
    var removed = owners.pop()
    var owner = Memory::move(removed).unwrap()
    if (*owner.get() != 42) { return 3 }
    owners.clear()

    var mut values = Vector<String>()
    values.push(String("first"))
    values.push(String("last"))
    if (values.insert(0, String("start")).is_error()) { return 14 }
    if (!values.remove(1).unwrap().equals("first")) { return 15 }
    if (!values.set(0, String("replacement")).unwrap().equals("start")) { return 16 }
    values.shrink_to_fit()
    if (values.length() != 2 || !values.get(1).unwrap()->equals("last")) { return 17 }
    var boxed = Box<String>(String("boxed"))
    var extracted = Memory::move(boxed).into_inner()
    if (!extracted.equals("boxed")) { return 18 }

    var mut map = Map<i32, String>(same)
    map.set(1, String("first"))
    map.set(2, String("second"))
    var map_copy = map
    map.clear()
    var key: i32 = 2
    if (map_copy.length() != 2 || !map_copy.get(&key).unwrap()->equals("second")) { return 4 }

    var mut arguments = FormatArguments()
    do {
        var local = String("retained")
        arguments.add(&local)
        0
    }
    var rendered = format("{}", &arguments).unwrap()
    if (!rendered.equals("retained")) { return 5 }
    var mut binary_arguments = FormatArguments()
    binary_arguments.add(&copied)
    var binary_rendered = format("{}", &binary_arguments).unwrap()
    if (!binary_rendered.equals(&copied)) { return 19 }
    if (!String("9223372036854775808").parse_i64().is_error() ||
        !String("18446744073709551616").parse_u64().is_error() ||
        !String(" -1").parse_u64().is_error() || !String("1e9999").parse_f64().is_error()) { return 6 }

    if (File::write_bytes("owned_bytes.bin", &raw, 3).is_error()) { return 7 }
    var bytes = File::read_bytes("owned_bytes.bin").unwrap()
    var bytes_copy = bytes
    var file_text = File::read_to_string("owned_bytes.bin").unwrap()
    if (bytes_copy.length() != 3 || !file_text.equals(&copied)) { return 8 }
    if (File::remove("owned_bytes.bin").is_error()) { return 9 }
    if (!File::read_bytes("missing_file.bin").is_error()) { return 10 }
    if (!File::write_string("/dev/full", "failure").is_error()) { return 20 }

    var request: cstr = "GET /test HTTP/1.1\r\nHost: localhost\r\n\r\n"
    var parsed = RequestParser::parse(request as *u8, request.length())
    if (parsed.is_error() || !parsed.unwrap().path.equals("/test")) { return 11 }
    var partial = StringView::from_cstr("GET / HTTP/1.1\r\n")
    var incomplete = MessageFraming::body_length(&partial)
    if (incomplete.is_ok() || incomplete.unwrap_error().value != HttpParseError::Incomplete.value) { return 12 }
    var chunked = StringView::from_cstr("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n")
    if (!MessageFraming::body_length(&chunked).is_error()) { return 13 }
    return 0
}
