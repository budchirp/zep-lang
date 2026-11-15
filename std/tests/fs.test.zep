import std.fs.File
import std.fs.FileMode
import std.fs.SeekOrigin
import std.fs.Directory
import std.fs.Path
import std.text.StringView
import std.testing.Test

test "files and directories report checked resource failures" {
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
}

test "byte buffers copy binary file data and preserve write errors" {
    var mut raw: u8[3] = [97 as u8, 0 as u8, 98 as u8]
    File::write_bytes("owned_bytes.bin", &raw, 3).unwrap()
    var bytes = File::read_bytes("owned_bytes.bin").unwrap()
    var copy = bytes

    Test::expect(copy.length() == 3 && File::read_to_string("owned_bytes.bin").unwrap().equals(
        StringView((&raw) as cstr, 3)))

    File::remove("owned_bytes.bin").unwrap()

    Test::expect(File::write_string("/dev/full", "failure").is_error())
}
