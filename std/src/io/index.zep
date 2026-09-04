import std.ffi.c_exit
import std.ffi.c_fprintf
import std.ffi.c_standard_error
import std.ffi.printf
import std.core.display.Display
import std.text.string.String

public struct Console {
    public:
        static fn print(value: cstr) -> void {
            printf("%s", value)
        }

        static fn print(value: *String) -> void {
            Console::print(value->as_cstr())
        }

        static fn print(value: i32) -> void {
            printf("%d", value)
        }

        static fn print(value: i64) -> void {
            printf("%lld", value)
        }

        static fn print(value: boolean) -> void {
            Console::print(if (value) { "true" } else { "false" })
        }

        static fn print(value: *Display) -> void {
            var rendered = value->to_string()
            Console::print(&rendered)
        }

        static fn eprint(value: cstr) -> void {
            c_fprintf(c_standard_error, "%s", value)
        }

        static fn eprint(value: *String) -> void {
            Console::eprint(value->as_cstr())
        }

        static fn eprint(value: i32) -> void {
            c_fprintf(c_standard_error, "%d", value)
        }

        static fn eprint(value: i64) -> void {
            c_fprintf(c_standard_error, "%lld", value)
        }

        static fn eprint(value: boolean) -> void {
            Console::eprint(if (value) { "true" } else { "false" })
        }

        static fn eprint(value: *Display) -> void {
            var rendered = value->to_string()
            Console::eprint(&rendered)
        }

        static fn sprint(value: cstr) -> String {
            return String(value)
        }

        static fn sprint(value: *String) -> String {
            return String(value->as_cstr())
        }

        static fn sprint(value: i32) -> String {
            return String::from(value)
        }

        static fn sprint(value: i64) -> String {
            return String::from(value)
        }

        static fn sprint(value: boolean) -> String {
            return String::from(value)
        }

        static fn sprint(value: *Display) -> String {
            return value->to_string()
        }

        static fn panic(message: cstr) -> never {
            Console::eprint(message)
            Console::eprint("\n")
            c_exit(1)
        }
}
