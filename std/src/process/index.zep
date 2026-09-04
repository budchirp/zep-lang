import std.ffi.c_fgets
import std.ffi.c_getenv
import std.ffi.c_pclose
import std.ffi.c_popen
import std.ffi.c_system
import std.core.option.Option
import std.core.result.Result
import std.text.string.String

public enum ProcessError {
    StartFailed
    CommandFailed

    public:
        fn to_string() -> String {
            return when (*self) {
                ProcessError::StartFailed -> String("failed to start process"),
                ProcessError::CommandFailed -> String("process exited unsuccessfully"),
            }
        }
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
    public:
        static fn run(command: cstr) -> Result<i32, ProcessError> {
            var status = c_system(command)
            if (status < 0) {
                return Result<i32, ProcessError>::Error { error: ProcessError::StartFailed }
            }

            if (status != 0) {
                return Result<i32, ProcessError>::Error { error: ProcessError::CommandFailed }
            }

            return Result<i32, ProcessError>::Ok { value: status }
        }

        static fn capture(command: cstr) -> Result<ProcessOutput, ProcessError> {
            var stream = c_popen(command, "r")
            if (stream == null) {
                return Result<ProcessOutput, ProcessError>::Error { error: ProcessError::StartFailed }
            }

            var mut output = String()
            var mut buffer: char[256]
            var mut line = c_fgets(&mut buffer as *mut void, 256, stream)

            while ((line as *mut void) != null) {
                output.append(line)
                line = c_fgets(&mut buffer as *mut void, 256, stream)
            }

            var status = c_pclose(stream)
            if (status < 0) {
                return Result<ProcessOutput, ProcessError>::Error { error: ProcessError::CommandFailed }
            }

            return Result<ProcessOutput, ProcessError>::Ok {
                value: ProcessOutput { status: status, output: output }
            }
        }
}

public struct Environment {
    public:
        static fn get(name: cstr) -> Option<String> {
            var value = c_getenv(name)
            if ((value as *mut void) == null) {
                return Option<String>::None
            }

            return Option<String>::Some { value: String(value) }
        }

        static fn home() -> Option<String> {
            return Environment::get("HOME")
        }
}
