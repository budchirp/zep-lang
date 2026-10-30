import std.net.http.server.HttpServer
import std.net.http.parser.RequestParser

public fn main() -> i32 {
    if (#sizeof(HttpServer) == 0 || #sizeof(RequestParser) == 0) {
        return 1
    }

    return 0
}
