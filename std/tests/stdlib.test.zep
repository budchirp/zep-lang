import std.collections.Vector
import std.collections.Map
import std.core.Box
import std.core.Pair
import std.fs.File
import std.fs.FileMode
import std.fs.SeekOrigin
import std.fs.Directory
import std.fs.Path
import std.memory.Memory
import std.math.Math
import std.process.Process
import std.text.String
import std.text.StringView
import std.algorithms.Algorithms
import std.algorithms.Ordering
import std.io.FormatArguments
import std.io.format
import std.net.http.framing.MessageFraming
import std.net.http.framing.HttpParseError
import std.net.http.parser.RequestParser
import std.testing.Test

fn compare(left: &i32, right: &i32) -> Ordering {
    return when {
        (left < right) -> Ordering::Less,
        (left > right) -> Ordering::Greater,
        else -> Ordering::Equal,
    }
}

fn same(left: &i32, right: &i32) -> boolean {
    return left == right
}

test "resources compose with owned text containers and checked I/O" {
    Test::expect(Math::absolute(-4) == 4 && Math::absolute(-4 as i64) == 4 &&
        Math::minimum(4 as i64, 7 as i64) == 4 && Math::maximum(0.5, 0.25) == 0.5 &&
        Math::clamp(2.0 as f32, 0.0 as f32, 1.0 as f32) == 1.0 as f32)

    var mut text = String("abc")
    text.append(text.as_view().substring(1, 3).unwrap())

    Test::expect(text.equals("abcbc") && String().substring(0, 0).unwrap().is_empty())

    var mut values: Vector<String> = Vector()
    values.push(String("first"))
    values.resize(8, *values.get(0).unwrap())
    values.truncate(2)

    Test::expect(values.length() == 2 && values.get(1).unwrap()->equals("first"))

    values.clear()
    values.resize(3, String("again"))

    var owner = Box(String("owned"))
    var raw = Memory::move(owner).into_raw()
    var adopted = Box::from_raw(raw)

    Test::expect(adopted.get()->equals("owned"))

    var pair = Pair(Box(1), Box(2))

    Test::expect(*pair.first.get() == 1 && *pair.second.get() == 2)
    Test::expect(String::from(-9223372036854775807 as i64 - 1).equals("-9223372036854775808") &&
        String::from(18446744073709551615 as u64).equals("18446744073709551615"))

    var captured = Process::capture(String("printf 'a\\000b'; exit 7")).unwrap()

    Test::expect(captured.status == 7 && !captured.succeeded() && captured.output.length() == 3 &&
        captured.output.get(1).unwrap() == 0 as char &&
        Process::run(StringView::from_cstr("exit 7")).unwrap() == 7)

    Directory::create_all("nested/inner").unwrap()
    File::write_string("nested/inner/data", "hello").unwrap()

    var mut file = File::open("nested/inner/data", FileMode::Read).unwrap()
    file.seek(2, SeekOrigin::Start).unwrap()
    var mut byte: u8 = 0

    Test::expect(file.position().unwrap() == 2 && file.read(&mut byte, 1).unwrap() == 1 && byte == 'l' as u8)
    Test::expect(file.close().is_ok() && file.read(&mut byte, 1).is_error())

    var entries = Directory::list("nested/inner").unwrap()

    Test::expect(entries.length() == 1 && entries.get(0).unwrap()->name.equals("data") &&
        !entries.get(0).unwrap()->is_directory)

    var mut directory = Directory::open("nested/inner").unwrap()
    var entry = directory.next().unwrap().unwrap()
    directory.close().unwrap()

    Test::expect(entry.name.equals("data") && directory.next().is_error() &&
        Directory::list("nested/inner/data").is_error())
    Test::expect(Directory::create_all("nested/inner/data/child").is_error())
    Test::expect(File::read_bytes("missing").unwrap_error().to_string().starts_with("failed to open file"))
    Test::expect(Path::join("nested", "inner").equals("nested/inner") &&
        Path::dirname("/data").equals("/") && Path::dirname("/tmp/data").equals("/tmp") &&
        Path::dirname("data").equals(".") && Path::basename("a/b").equals("b") &&
        Path::absolute("nested").is_ok())

    File::remove("nested/inner/data").unwrap()
    File::remove("nested/inner").unwrap()
    File::remove("nested").unwrap()

    var mut numbers: Vector<i32> = Vector()
    numbers.push(3)
    numbers.push(1)
    numbers.push(2)
    Algorithms::sort(numbers, compare)

    Test::expect(Algorithms::binary_search(numbers, 2, compare).unwrap() == 1 &&
        Algorithms::binary_search(numbers, 0, compare).is_none() &&
        Algorithms::binary_search(numbers, 4, compare).is_none() &&
        Algorithms::find(numbers, 3, same).unwrap() == 2)

    Algorithms::reverse(numbers)
    Test::expect(*numbers.first().unwrap() == 3)
}

test "owned values preserve binary text and propagate operational errors" {
    var mut raw: u8[3] = [97 as u8, 0 as u8, 98 as u8]
    var view = StringView((&raw) as cstr, 3)
    var mut text = String(view)
    var copied = text
    text.append(text)

    Test::expect(text.length() == 6 && copied.length() == 3 && copied.get(2).unwrap() == 'b')

    var moved = Memory::move(text)
    text.append("reused")

    Test::expect(moved.length() == 6 && text.equals("reused"))

    var repeated = copied.repeat(3).unwrap()

    Test::expect(repeated.length() == 9 && repeated.get(7).unwrap() == 0 as char)
    Test::expect(copied.replace("", "x").equals(copied))

    var zero = StringView((&raw[1]) as cstr, 1)
    var replacement = StringView::from_cstr("!")

    Test::expect(copied.replace(zero, replacement).equals("a!b"))
    Test::expect(String("MiXeD").to_lowercase().equals("mixed") &&
        String("MiXeD").to_uppercase().equals("MIXED"))

    var mut owners: Vector<Box<i32>> = Vector()
    owners.push(Box(42))
    owners.push(Box(7))
    owners.swap(0, 1)
    var removed = owners.pop()
    var owner = Memory::move(removed).unwrap()

    Test::expect(*owner.get() == 42)
    owners.clear()

    var mut values: Vector<String> = Vector()
    values.push(String("first"))
    values.push(String("last"))
    values.insert(0, String("start")).unwrap()

    Test::expect(values.remove(1).unwrap().equals("first"))
    Test::expect(values.set(0, String("replacement")).unwrap().equals("start"))
    values.shrink_to_fit()

    Test::expect(values.length() == 2 && values.get(1).unwrap()->equals("last"))

    var boxed = Box(String("boxed"))

    Test::expect(Memory::move(boxed).into_inner().equals("boxed"))

    var mut map: Map<i32, String> = Map(same)
    map.set(1, String("first"))
    map.set(2, String("second"))
    var map_copy = map
    map.clear()
    var key: i32 = 2

    Test::expect(map_copy.length() == 2 && map_copy.get(key).unwrap()->equals("second"))

    var mut arguments = FormatArguments()
    do {
        var local = String("retained")
        arguments.add(local)
        0
    }

    Test::expect(format("{}", arguments).unwrap().equals("retained"))
    arguments.clear()
    arguments.add(copied)

    Test::expect(format("{}", arguments).unwrap().equals(copied))
    Test::expect(String("9223372036854775808").parse_i64().is_error() &&
        String("18446744073709551616").parse_u64().is_error() &&
        String(" -1").parse_u64().is_error() && String("1e9999").parse_f64().is_error())

    File::write_bytes("owned_bytes.bin", &raw, 3).unwrap()
    var bytes = File::read_bytes("owned_bytes.bin").unwrap()
    var bytes_copy = bytes

    Test::expect(bytes_copy.length() == 3 && File::read_to_string("owned_bytes.bin").unwrap().equals(copied))
    File::remove("owned_bytes.bin").unwrap()

    Test::expect(File::read_bytes("missing_file.bin").is_error())
    Test::expect(File::write_string("/dev/full", "failure").is_error())

    var parsed = RequestParser::parse(StringView::from_cstr("GET /test HTTP/1.1\r\nHost: localhost\r\n\r\n"))

    Test::expect(parsed.is_ok() && parsed.unwrap().path.equals("/test"))

    var incomplete = MessageFraming::body_length(StringView::from_cstr("GET / HTTP/1.1\r\n"))

    Test::expect(incomplete.is_error() && incomplete.unwrap_error().value == HttpParseError::Incomplete.value)
    Test::expect(MessageFraming::body_length(StringView::from_cstr(
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n")).is_error())
}

test "formatting arguments grow clear and retain all numeric overloads" {
    var mut arguments = FormatArguments()
    var mut output = String()

    Test::expect(arguments.is_empty() && arguments.length() == 0 &&
        arguments.append_to(-1, output).is_error() && arguments.append_to(0, output).is_error())

    for (var mut index = 0; index < 40; index = index + 1) {
        arguments.add(index)
    }

    Test::expect(!arguments.is_empty() && arguments.length() == 40 && arguments.append_to(40, output).is_error())

    for (var mut index = 0; index < 40; index = index + 1) {
        output.clear()
        arguments.append_to(index as i64, output).unwrap()
        Test::expect(output.equals(String::from(index)))
    }

    arguments.clear()
    Test::expect(arguments.is_empty() && arguments.length() == 0 && arguments.append_to(0, output).is_error())

    arguments.add("borrowed")
    arguments.add(String("owned"))
    arguments.add(-1 as i8)
    arguments.add(-2 as i16)
    arguments.add(-3 as i32)
    arguments.add(-4 as i64)
    arguments.add(1 as u8)
    arguments.add(2 as u16)
    arguments.add(3 as u32)
    arguments.add(4 as u64)
    arguments.add(1.5 as f32)
    arguments.add(2.5 as f64)
    arguments.add(true)
    arguments.add('Z')

    Test::expect(format("{} {} {} {} {} {} {} {} {} {} {} {} {} {}", arguments).unwrap().equals(
        "borrowed owned -1 -2 -3 -4 1 2 3 4 1.5 2.5 true Z"))
}
