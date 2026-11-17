import std.ffi.pipe.Pipe
import std.ffi.directory.DirectoryHandle
import std.fs.File
import std.fs.FileMode
import std.fs.FileError
import std.io.error.IoError
import std.memory.Memory

var mut pipes = 0
var mut descriptors = 0
var mut directories = 0
var mut failed = false

@name("popen")
public fn open_pipe(command: cstr, mode: cstr) -> *mut void {
    return 1 as *mut void
}

@name("pclose")
public fn close_pipe(handle: *mut void) -> i32 {
    pipes = pipes + 1

    return if (failed) { -1 } else { 0 }
}

@name("close")
public fn close_descriptor(descriptor: i32) -> i32 {
    descriptors = descriptors + 1

    return 0
}

@name("open")
public fn open_descriptor(path: cstr, flags: i32, mode: u32) -> i32 {
    return 43
}

@name("opendir")
public fn open_directory(path: cstr) -> *mut void {
    return 2 as *mut void
}

@name("closedir")
public fn close_directory(handle: *mut void) -> i32 {
    directories = directories + 1

    return 0
}

@throws(IoError)
fn transfer() -> void {
    var source = try Pipe::open("ignored")
    var moved = Memory::move(source)
    var descriptor = File(42)
    var adopted = Memory::move(descriptor)
    var directory = try DirectoryHandle::open("ignored")
    var retained = Memory::move(directory)
}

@throws(FileError)
fn early_return() -> i32 {
    var file = try File::open("ignored", FileMode::Read)

    return 7
}

public fn main() -> i32 {
    return try {
        transfer()

        if (pipes != 1 || descriptors != 1 || directories != 1) {
            return 1
        }

        if (early_return() != 7 || pipes != 1 || descriptors != 2) {
            return 2
        }

        do {
            var mut pipe = Pipe::open("ignored")
            failed = true

            try {
                pipe.close()
                return 3
            } catch { error: IoError -> {}, }

            if (pipe.as_descriptor() != -1) {
                return 4
            }

            pipe.close()
            0
        }

        when (pipes) { 2 -> 0, else -> 6, }
    } catch {
        error: IoError -> 5,
        error: FileError -> 7,
    }
}
