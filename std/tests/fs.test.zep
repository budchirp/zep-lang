import std.fs.File
import std.fs.FileMode
import std.fs.SeekOrigin
import std.fs.Directory
import std.fs.Path
import std.fs.FileError
import std.fs.FileOperation
import std.io.error.IoError
import std.io.error.IoCode
import std.text.StringView
import std.testing.Test

test "files and directories report checked resource failures" {
    try {
        Directory::create_all("nested/inner")
        File::write_string("nested/inner/data", "hello")

        var mut file = File::open("nested/inner/data", FileMode::Read)
        file.seek(2, SeekOrigin::Start)
        var mut byte: u8 = 0

        Test::expect(file.position() == 2 && file.read(&mut byte, 1) == 1 && byte == 'l' as u8)
        file.close()

        try {
            file.read(&mut byte, 1)
            Test::fail("closed file was readable")
        } catch { error: IoError -> { Test::expect(error.value == IoCode::Closed.value) }, }

        var entries = Directory::list("nested/inner")

        Test::expect(entries.length() == 1 && entries.get(0).unwrap()->name.equals("data") &&
            !entries.get(0).unwrap()->is_directory)

        var mut directory = Directory::open("nested/inner")
        var entry = directory.next().unwrap()
        directory.close()
        Test::expect(entry.name.equals("data"))

        try {
            directory.next()
            Test::fail("closed directory was readable")
        } catch { error: FileError -> { Test::expect(error.operation == FileOperation::ReadDirectory) }, }

        try {
            Directory::list("nested/inner/data")
            Test::fail("file was accepted as a directory")
        } catch { error: FileError -> { Test::expect(error.operation == FileOperation::ReadDirectory) }, }

        try {
            Directory::create_all("nested/inner/data/child")
            Test::fail("file was accepted as a parent directory")
        } catch { error: FileError -> { Test::expect(error.operation == FileOperation::CreateDirectory) }, }

        try {
            File::read_bytes("missing")
            Test::fail("missing file was readable")
        } catch { error: FileError -> {
            Test::expect(error.to_string().starts_with("failed to open file"))
        }, }

        Test::expect(Path::join("nested", "inner").equals("nested/inner") &&
            Path::dirname("/data").equals("/") && Path::dirname("/tmp/data").equals("/tmp") &&
            Path::dirname("data").equals(".") && Path::basename("a/b").equals("b") &&
            !Path::absolute("nested").is_empty())

        File::remove("nested/inner/data")
        File::remove("nested/inner")
        File::remove("nested")
    } catch {
        error: FileError -> Test::fail("unexpected file failure"),
        error: IoError -> Test::fail("unexpected I/O failure"),
    }
}

test "byte buffers copy binary file data and preserve write errors" {
    try {
        var mut raw: u8[3] = [97 as u8, 0 as u8, 98 as u8]
        File::write_bytes("owned_bytes.bin", &raw, 3)
        var bytes = File::read_bytes("owned_bytes.bin")
        var copy = bytes

        Test::expect(copy.length() == 3 && File::read_to_string("owned_bytes.bin").equals(
            StringView((&raw) as cstr, 3)))

        File::remove("owned_bytes.bin")

        try {
            File::write_string("/dev/full", "failure")
            Test::fail("full device accepted a write")
        } catch { error: FileError -> { Test::expect(error.operation == FileOperation::Write) }, }
    } catch { error: FileError -> Test::fail("unexpected file failure"), }
}
