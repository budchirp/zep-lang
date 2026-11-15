import std.io.print
import std.io.eprint
import std.io.format
import std.io.FormatArguments
import std.io.console.StandardFile
import std.ffi.zep_errno
import std.collections.Vector
import std.fs.File
import std.fs.FileMode
import std.text.String
import std.text.StringView

var mut received: char[64]
var mut length: i64 = 0
var mut calls = 0
var mut stopped = false

@name("write")
public fn write_chunk(descriptor: i32, data: *void, count: i64) -> i64 {
    calls = calls + 1

    if (stopped) {
        return 0
    }

    if (calls == 1) {
        var error = zep_errno()
        error[0] = 4

        return -1
    }

    var copied = when { (count > 2) -> 2 as i64, else -> count, }
    var bytes = data as *char

    for (var mut index: i64 = 0; index < copied; index = index + 1) {
        received[length] = bytes[index]
        length = length + 1
    }

    return copied
}

public fn main() -> i32 {
    var raw: char[5] = ['a', 0 as char, '{', '}', 'b']
    var pattern = StringView((&raw) as cstr, 5)
    var mut arguments = FormatArguments()
    arguments.add(String("X"))
    var formatted = format(pattern, arguments).unwrap()

    if (formatted.length() != 4 || !formatted.as_view().contains(0 as char)) {
        return 1
    }

    if (print("{literal}").unwrap() != 9 || print(formatted).unwrap() != 4 ||
        eprint(pattern).unwrap() != 5 || StandardFile::write(StandardFile::Output, String("ok")).unwrap() != 2) {
        return 2
    }

    if (calls != 12 || length != 20 || received[9] != 'a' || received[10] != 0 as char ||
        received[11] != 'X' || received[17] != 'b' || received[19] != 'k') {
        return 3
    }

    var escaped = String("{{{}}}")

    if (!format(escaped, arguments).unwrap().equals("{X}")) {
        return 4
    }

    var mut values: Vector<String> = Vector()

    if (values.reserve(-1).is_ok() || values.truncate(-1).is_ok() ||
        values.resize(-1, String("x")).is_ok() || values.reserve(9223372036854775807 as i64).is_ok()) {
        return 5
    }

    var mut text = String("xx")

    if (text.reserve(-1).is_ok() || text.reserve(9223372036854775807 as i64).is_ok() ||
        text.repeat(-1).is_ok() || text.repeat(9223372036854775807 as i64).is_ok() ||
        !String().repeat(9223372036854775807 as i64).unwrap().is_empty()) {
        return 6
    }

    if (File::open(pattern, FileMode::Read).is_ok() ||
        !text.starts_with(StringView::from_cstr("x")) || !text.ends_with(String("x")) ||
        text.index_of(String("x"), 0).unwrap() != 0 || !String::from(1.5).unwrap().equals("1.5")) {
        return 7
    }

    var path = String("text_io.bin")
    var mut file = File::open(path, FileMode::Write).unwrap()
    file.close().unwrap()
    File::remove(path.as_cstr()).unwrap()

    stopped = true
    var failed = StandardFile::write(StandardFile::Output, "x")

    if (failed.is_ok() || failed.unwrap_error().value != 5) {
        return 8
    }

    var invalid = StandardFile::write_bytes(StandardFile::Output, null, 1)

    if (invalid.is_ok() || invalid.unwrap_error().value != 22) {
        return 9
    }

    return 0
}
