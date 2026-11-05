import std.net.http.HttpServer
import std.memory.Memory
import std.net.http.HttpClient
import std.net.http.Request
import std.net.http.Response
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
    var connection = TcpStream::connect(Ipv4Address::localhost(), port)
    if (connection.is_error()) { return 1 }
    var mut stream = Memory::move(connection).unwrap()
    var first: cstr = "GET / HTTP/1.1\r\nHost: loopback\r\nContent-Length: 4\r\n"
    if (stream.write_all(first, first.length()).is_error()) { return 2 }
    usleep(20000 as u32)
    if (stream.write_all("\r\nab", 4).is_error()) { return 3 }
    usleep(20000 as u32)
    if (stream.write_all("cd", 2).is_error()) { return 4 }
    var mut buffer: u8[1024]
    var received = stream.read(&mut buffer, 1024)
    if (received.is_error() || received.unwrap() == 0) { return 5 }
    stream.close()
    var result = HttpClient::get(Ipv4Address::localhost(), "separate-host-header", port, "/")
    if (result.is_error()) { return 6 }
    var response = result.unwrap()
    if (response.status != 200 || !response.body.equals("loopback")) { return 7 }
    return 0
}

public fn main() -> i32 {
    alarm(10 as u32)
    var binding = HttpServer::bind(0 as u16)
    if (binding.is_error()) { return 1 }
    var mut server = Memory::move(binding).unwrap()
    server.get("/", respond)
    var port = server.local_port().unwrap()
    var process = fork()
    if (process < 0) { return 2 }
    if (process == 0) { exit_child(client(port)) }
    if (server.serve_one().is_error()) { return 3 }
    if (server.serve_one().is_error()) { return 4 }
    var mut status = 0
    if (waitpid(process, &mut status, 0) != process || status != 0) { return 5 }
    return 0
}
