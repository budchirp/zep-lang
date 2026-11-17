import std.net.http.HttpServer
import std.net.http.HttpClient
import std.net.http.Request
import std.net.http.Response
import std.net.http.HttpError
import std.net.http.HttpParseError
import std.io.error.IoError
import std.net.socket.TcpStream
import std.net.socket.Ipv4Address

extern fn fork() -> i32
extern fn waitpid(process: i32, status: *mut i32, options: i32) -> i32
extern fn usleep(duration: u32) -> i32
extern fn alarm(seconds: u32) -> u32
@name("_exit")
extern fn exit_child(status: i32) -> never

fn respond(request: *Request, response: *mut Response) -> void {
    response->text("loopback")
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
        var response = HttpClient::get(Ipv4Address::localhost(), "separate-host-header", port, "/")

        if (response.status != 200 || !response.body.equals("loopback")) { return 7 }

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
        var mut server = HttpServer::bind(0 as u16)
        server.get("/", respond)
        var port = server.local_port()
        var process = fork()

        if (process < 0) { return 2 }
        if (process == 0) { exit_child(client(port)) }

        server.serve_one()
        server.serve_one()
        var mut status = 0

        if (waitpid(process, &mut status, 0) != process || status != 0) { return 5 }

        0
    } catch {
        error: IoError -> 3,
        error: HttpParseError -> 4,
    }
}
