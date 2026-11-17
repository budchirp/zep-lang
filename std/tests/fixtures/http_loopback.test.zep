import std.net.http.HttpServer
import std.net.http.HttpClient
import std.net.http.Request
import std.net.http.Response
import std.net.http.Method
import std.net.http.Handler
import std.net.http.Status
import std.net.http.HttpError
import std.net.http.HttpParseError
import std.io.error.IoError
import std.net.socket.TcpStream
import std.net.socket.Ipv4Address
import std.text.String
import std.text.StringView

extern fn fork() -> i32
extern fn waitpid(process: i32, status: *mut i32, options: i32) -> i32
extern fn usleep(duration: u32) -> i32
extern fn alarm(seconds: u32) -> u32
@name("_exit")
extern fn exit_child(status: i32) -> never

struct Echo : Handler {
    public:
        var calls: i64

        override fn handle(request: *Request, response: *mut Response) mut -> void {
            calls = calls + 1

            when (request->method) {
                Method::Get -> response->text("loopback"),
                Method::Post -> response->status(Status::Created)->text(request->body),
                Method::Put -> response->text(request->body),
                Method::Delete -> response->text(String::from(calls)),
                else -> response->status(Status::BadRequest),
            }
        }
}

fn client(port: u16) -> i32 {
    return try {
        var mut stream = TcpStream::connect(Ipv4Address::localhost(), port)
        var first: cstr = "GET / HTTP/1.1\r\nHost: loopback\r\nContent-Length: 4\r\n"
        stream.file.write(first)
        usleep(20000 as u32)
        stream.file.write("\r\nab", 4)
        usleep(20000 as u32)
        stream.file.write("cd", 2)
        var mut buffer: u8[1024]

        if (stream.file.read(&mut buffer, 1024) == 0) { return 5 }

        stream.file.close()
        var client = HttpClient(Ipv4Address::localhost(), "separate-host-header", port)
        var response = try client.handle(Method::Get, "/")

        if (response.status != 200 || !response.body.equals("loopback")) { return 7 }

        var raw: char[5] = ['a', 0 as char, 'b', '\n', 'c']
        var body = StringView((&raw) as cstr, #length(raw))
        var created = client.handle(Method::Post, "/items/42", body)

        if (created.status != 201 || !created.body.equals(body)) { return 9 }

        var updated = client.handle(Method::Put, "/items/42", String("updated"))

        if (updated.status != 200 || !updated.body.equals("updated")) { return 10 }
        if (!client.handle(Method::Delete, "/items/42").body.equals("5")) { return 11 }
        if (client.handle(Method::Get, "/items/42").status != 405) { return 12 }
        if (client.handle(Method::Post, "/missing", "text").status != 404) { return 13 }
        if (client.handle(Method::Get, "/items").status != 404) { return 14 }

        0
    } catch {
        error: IoError -> 1,
        error: HttpError -> 6,
        error: HttpParseError -> 8,
    }
}

public fn main() -> i32 {
    return try {
        alarm(10 as u32)
        var mut echo = Echo { calls: 0 }
        var mut server = HttpServer::bind(0 as u16)
        server.handle(Method::Get, "/", echo)
        server.handle(Method::Post, "/items/*", echo)
        server.handle(Method::Put, "/items/*", echo)
        server.handle(Method::Delete, "/items/*", echo)
        var port = server.local_port()
        var process = fork()

        if (process < 0) { return 2 }
        if (process == 0) { exit_child(client(port)) }

        for (var mut index = 0; index < 8; index = index + 1) {
            server.serve_one()
        }

        if (echo.calls != 5) { return 6 }
        var mut status = 0

        if (waitpid(process, &mut status, 0) != process || status != 0) { return 5 }

        0
    } catch {
        error: IoError -> 3,
        error: HttpParseError -> 4,
    }
}
