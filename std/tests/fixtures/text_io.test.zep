import std.io.print
import std.io.eprint
import std.io.format
import std.io.FormatArguments
import std.io.FormatError
import std.io.error.IoError
import std.io.console.StandardFile
import std.ffi.zep_errno
import std.collections.Vector
import std.collections.CapacityError
import std.collections.IndexError
import std.fs.File
import std.fs.FileMode
import std.fs.FileError
import std.text.String
import std.text.StringView
import std.text.ConversionError

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
    return try {
        var raw: char[5] = ['a', 0 as char, '{', '}', 'b']
        var pattern = StringView((&raw) as cstr, 5)
        var mut arguments = FormatArguments()
        arguments.add(String("X"))
        var formatted = format(pattern, arguments)

        if (formatted.length() != 4 || !formatted.as_view().contains(0 as char)) {
            return 1
        }

        print("{literal}")
        print(formatted)
        eprint(pattern)

        if (StandardFile::write(StandardFile::Output, String("ok")) != 2) {
            return 2
        }

        if (calls != 12 || length != 20 || received[9] != 'a' || received[10] != 0 as char ||
            received[11] != 'X' || received[17] != 'b' || received[19] != 'k') {
            return 3
        }

        print("{", arguments)

        if (length != 21 || received[20] != '{') {
            return 15
        }

        var escaped = String("{{{}}}")

        if (!format(escaped, arguments).equals("{X}")) {
            return 4
        }

        var mut values: Vector<String> = Vector()
        var mut text = String("xx")
        var limits: i64[2] = [-1, 9223372036854775807 as i64]

        for (var mut index = 0; index < 2; index = index + 1) {
            try {
                values.reserve(limits[index])
                return 5
            } catch { error: CapacityError -> {}, }

            try {
                text.reserve(limits[index])
                return 6
            } catch { error: CapacityError -> {}, }

            try {
                text.repeat(limits[index])
                return 6
            } catch { error: CapacityError -> {}, }
        }

        try {
            values.truncate(-1)
            return 5
        } catch { error: IndexError -> {}, }

        try {
            values.resize(-1, String("x"))
            return 5
        } catch { error: CapacityError -> {}, }

        if (!String().repeat(9223372036854775807 as i64).is_empty()) {
            return 6
        }

        try {
            File::open(pattern, FileMode::Read)
            return 7
        } catch { error: FileError -> { if (error.error.value != 22) { return 7 } }, }

        if (!text.starts_with(StringView::from_cstr("x")) || !text.ends_with(String("x")) ||
            text.index_of(String("x"), 0).unwrap() != 0 || !String::from(1.5).equals("1.5")) {
            return 7
        }

        var path = String("text_io.bin")
        var mut file = File::open(path, FileMode::Write)
        file.close()
        File::remove(path.as_cstr())
        stopped = true

        print("ignored output failure")
        eprint("ignored error output failure")
        print("{}", arguments)

        try {
            StandardFile::write(StandardFile::Output, "x")
            return 8
        } catch { error: IoError -> { if (error.value != 5) { return 8 } }, }

        try {
            StandardFile::write_bytes(StandardFile::Output, null, 1)
            return 9
        } catch { error: IoError -> { if (error.value != 22) { return 9 } }, }

        0
    } catch {
        error: FormatError -> 10,
        error: CapacityError -> 11,
        error: FileError -> 12,
        error: ConversionError -> 13,
        error: IoError -> 14,
    }
}
