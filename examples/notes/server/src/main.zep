import api.Api
import std.io.print
import std.io.eprint
import std.io.error.IoError
import std.net.http.HttpServer
import std.net.http.HttpParseError

public fn main() -> i32 {
    return try {
        var mut api = Api()

        var mut server = HttpServer::bind(8080)
        api.register(&mut server)
        
        print("Notes server: http://127.0.0.1:8080\n")
        server.serve()
    } catch {
        error: IoError -> {
            eprint("notes server: I/O failure\n")

            1
        },
        error: HttpParseError -> {
            eprint("notes server: invalid HTTP request\n")

            1
        },
    }
}
