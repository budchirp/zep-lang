import std.ffi.pipe.Pipe
import std.fs.file.File
import std.ffi.c_getenv
import std.ffi.c_system
import std.ffi.zep_errno
import std.io.error.IoError
import std.io.error.IoCode
import std.core.option.Option
import std.core.error.Error
import std.memory.Memory
import std.text.string.String
import std.text.view.StringView

public enum ProcessOperation : i32 {
    Start = 0
    Read = 1
    Wait = 2
}

public struct ProcessError : Error {
    public:
        var operation: ProcessOperation
        var error: IoError
}

public struct ProcessOutput {
    public:
        var status: i32
        var output: String

        fn succeeded() -> boolean {
            return status == 0
        }
}

public struct Process {
    private:
        static fn exit_code(status: i32) -> i32 {
            var signal = status % 128

            return when (signal) {
                0 -> (status / 256) % 256,
                else -> 128 + signal,
            }
        }

    public:
        @throws(ProcessError)
        static fn run(command: cstr) -> i32 {
            var status = c_system(command)

            return when {
                (status < 0) -> throw ProcessError { operation: ProcessOperation::Start,
                    error: IoError { value: *zep_errno() } },
                else -> Process::exit_code(status),
            }
        }

        @throws(ProcessError)
        static fn run(command: &StringView) -> i32 {
            if (command.contains(0 as char)) {
                throw ProcessError { operation: ProcessOperation::Start,
                    error: IoError { value: IoCode::InvalidInput.value } }
            }

            return try Process::run(String(command).as_cstr())
        }

        @throws(ProcessError)
        static fn run(command: &String) -> i32 {
            return try Process::run(command.as_view())
        }

        @throws(ProcessError)
        static fn capture(command: &StringView) -> ProcessOutput {
            if (command.contains(0 as char)) {
                throw ProcessError { operation: ProcessOperation::Start,
                    error: IoError { value: IoCode::InvalidInput.value } }
            }

            return try Process::capture(String(command).as_cstr())
        }

        @throws(ProcessError)
        static fn capture(command: &String) -> ProcessOutput {
            return try Process::capture(command.as_view())
        }

        @throws(ProcessError)
        static fn capture(command: cstr) -> ProcessOutput {
            var mut pipe = Pipe::open(command) catch {
                error: IoError -> throw ProcessError { operation: ProcessOperation::Start, error: error },
            }
            var file = File::borrow(pipe.as_descriptor())
            var mut output = String()
            var mut buffer: char[4096]

            while (true) {
                var count = file.read(&mut buffer, #length(buffer)) catch {
                    error: IoError -> throw ProcessError { operation: ProcessOperation::Read, error: error },
                }

                if (count == 0) {
                    var status = pipe.close() catch {
                        error: IoError -> throw ProcessError { operation: ProcessOperation::Wait, error: error },
                    }

                    return ProcessOutput { status: Process::exit_code(status), output: Memory::move(output) }
                }

                output.append(StringView((&buffer) as cstr, count))
            }
        }
}

public struct Environment {
    public:
        static fn get(name: cstr) -> Option<String> {
            var value = c_getenv(name)

            return when (value as *void) {
                null -> Option::None,
                else -> Option::Some { value: String(value) },
            }
        }
}
