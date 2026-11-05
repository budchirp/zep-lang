import std.fs.writer.StandardFile
import std.ffi.zep_errno

var mut calls = 0
var mut received: u8[6]
var mut received_length: i64 = 0
var mut stopped = false

@name("write")
public fn scripted_write(descriptor: i32, data: *void, length: i64) -> i64 {
    calls = calls + 1
    if (stopped) { return 0 }
    if (calls == 1) {
        var error = zep_errno()
        error[0] = 4
        return -1
    }
    var count = if (length > 2) { 2 as i64 } else { length }
    var bytes = data as *u8
    for (var mut index: i64 = 0; index < count; index = index + 1) {
        received[received_length] = bytes[index]
        received_length = received_length + 1
    }
    return count
}

public fn main() -> i32 {
    var result = StandardFile::write(StandardFile::Output, "abcdef")
    if (result.is_error() || result.unwrap() != 6 || calls != 4 || received_length != 6) { return 1 }
    if (received[0] != 'a' as u8 || received[5] != 'f' as u8) { return 2 }
    stopped = true
    var failed = StandardFile::write(StandardFile::Output, "x")
    if (failed.is_ok() || failed.unwrap_error().code != 5) { return 3 }
    var invalid = StandardFile::write_bytes(StandardFile::Output, null, 1)
    if (invalid.is_ok() || invalid.unwrap_error().code != 22) { return 4 }
    return 0
}
