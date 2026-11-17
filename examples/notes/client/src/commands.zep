import std.core.Option
import std.memory.Memory
import std.net.http.Method
import std.text.String

public struct Command {
    public:
        var method: Method
        var path: String
        var body: String

        static fn parse(count: i32, arguments: *cstr) -> Option<Command> {
            if (count < 2) {
                return Option::None
            }

            var command = arguments[1]
            var method = when {
                (command.equals("list") && count == 2) -> Method::Get,
                (command.equals("read") && count == 3) -> Method::Get,
                (command.equals("create") && count == 3) -> Method::Post,
                (command.equals("update") && count == 4) -> Method::Put,
                (command.equals("delete") && count == 3) -> Method::Delete,
                else -> { return Option::None },
            }
            var mut path = String("/notes")

            if (count >= 3 && method != Method::Post) {
                path.append("/")
                path.append(arguments[2])
            }

            return Option::Some { value: Command {
                method: method,
                path: Memory::move(path),
                body: String(when (method) {
                    Method::Post -> arguments[2],
                    Method::Put -> arguments[3],
                    else -> "",
                }),
            } }
        }
}
