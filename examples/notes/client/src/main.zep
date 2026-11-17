import commands.Command
import std.core.Option
import std.memory.Memory
import std.io.print
import std.io.eprint
import std.net.http.HttpClient
import std.net.http.HttpError
import std.net.http.HttpParseError
import std.net.socket.Ipv4Address

public fn main(count: i32, arguments: *cstr) -> i32 {
    return try {
        var command = when (Command::parse(count, arguments)) {
            Option::Some { value } -> Memory::move(value),
            Option::None -> {
                eprint("usage: notes list | read ID | create TEXT | update ID TEXT | delete ID\n")

                return 2
            },
        }

        var client = HttpClient(Ipv4Address::localhost(), "localhost:8080", 8080)
        var response = client.handle(command.method, command.path.as_cstr(), command.body)
        print(response.body)

        if (!response.body.ends_with("\n")) {
            print("\n")
        }

        if (response.is_success()) { 0 } else { 1 }
    } catch {
        error: HttpError -> {
            eprint("notes: HTTP transport failed; is the server running?\n")

            1
        },
        error: HttpParseError -> {
            eprint("notes: invalid HTTP message\n")

            1
        },
    }
}
