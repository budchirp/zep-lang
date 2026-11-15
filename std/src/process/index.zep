import std.ffi.pipe.Pipe
import std.fs.file.File
import std.ffi.c_getenv
import std.ffi.c_system
import std.ffi.zep_errno
import std.io.error.IoError
import std.core.option.Option
import std.core.result.Result
import std.memory.Memory
import std.text.string.String
import std.text.view.StringView

public enum ProcessError(error: IoError) {
    Start
    Read
    Wait
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
        static fn run(command: cstr) -> Result<i32, ProcessError> {
            var status = c_system(command)

            return when {
                (status < 0) -> Result::Error { error: ProcessError::Start { error: *zep_errno() as IoError } },
                else -> Result::Ok { value: Process::exit_code(status) },
            }
        }

        static fn run(command: &StringView) -> Result<i32, ProcessError> {
            return when (command.contains(0 as char)) {
                true -> Result::Error { error: ProcessError::Start { error: IoError::InvalidInput } },
                false -> Process::run(String(command).as_cstr()),
            }
        }

        static fn run(command: &String) -> Result<i32, ProcessError> {
            return Process::run(command.as_view())
        }

        static fn capture(command: &StringView) -> Result<ProcessOutput, ProcessError> {
            return when (command.contains(0 as char)) {
                true -> Result::Error { error: ProcessError::Start { error: IoError::InvalidInput } },
                false -> Process::capture(String(command).as_cstr()),
            }
        }

        static fn capture(command: &String) -> Result<ProcessOutput, ProcessError> {
            return Process::capture(command.as_view())
        }

        static fn capture(command: cstr) -> Result<ProcessOutput, ProcessError> {
            var mut pipe = when (Pipe::open(command)) {
                Result::Error { error } -> { return Result::Error { error: ProcessError::Start { error: error } } },
                Result::Ok { value } -> Memory::move(value),
            }
            var file = File::borrow(pipe.as_descriptor())
            var mut output = String()
            var mut buffer: char[4096]

            while (true) {
                var count = when (file.read(&mut buffer, #length(buffer))) {
                    Result::Error { error } -> { return Result::Error { error: ProcessError::Read { error: error } } },
                    Result::Ok { value } -> value,
                }

                if (count == 0) {
                    return when (pipe.close()) {
                        Result::Error { error } -> Result::Error { error: ProcessError::Wait { error: error } },
                        Result::Ok { value } -> Result::Ok { value: ProcessOutput {
                            status: Process::exit_code(value), output: Memory::move(output)
                        } },
                    }
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
