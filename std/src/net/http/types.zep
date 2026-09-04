import std.ffi.c_snprintf
import std.text.string.String
import std.net.socket.stream.TcpStream
import std.net.socket.error.SocketError
import std.core.result.Result

public enum Method {
    Get
    Post
    Put
    Delete
    Unknown

    public:
        static fn parse(buffer: cstr) -> Method {
            if (buffer.starts_with("GET ")) { return Method::Get }
            if (buffer.starts_with("POST ")) { return Method::Post }
            if (buffer.starts_with("PUT ")) { return Method::Put }
            if (buffer.starts_with("DELETE ")) { return Method::Delete }
            return Method::Unknown
        }

        fn matches(other: Method) -> boolean { return (*self).value == other.value }

        fn path_start() -> i32 {
            return when (*self) {
                Method::Get -> 4, Method::Post -> 5, Method::Put -> 4, Method::Delete -> 7,
                else -> 0,
            }
        }
}

public enum Status : i32 {
    Ok = 200
    Created = 201
    BadRequest = 400
    NotFound = 404
    MethodNotAllowed = 405
    InternalServerError = 500

    public:
        fn text() -> cstr {
            return when (*self) {
                Status::Ok -> "OK", Status::Created -> "Created",
                Status::BadRequest -> "Bad Request", Status::NotFound -> "Not Found",
                Status::MethodNotAllowed -> "Method Not Allowed",
                else -> "Internal Server Error",
            }
        }
}

public type Handler = (*Request, *mut Response) -> void

public struct Request {
    public:
        var method: Method
        var path: String
        var body: String

}

public struct Response {
    public:
        var status: Status
        var content_type: String
        var body: String

        fn status(code: Status) mut -> *mut Response {
            self->status = code
            return self
        }

        fn text(body: cstr) mut -> *mut Response {
            content_type = String("text/plain")
            self->body = String(body)
            return self
        }

        fn html(body: cstr) mut -> *mut Response {
            content_type = String("text/html")
            self->body = String(body)
            return self
        }

        fn send_to(stream: *mut TcpStream) mut -> Result<i32, SocketError> {
            var mut header: u8[512]
            var body_length = body.length()
            var header_length = c_snprintf(
                &mut header, 512 as i64,
                "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %lld\r\nConnection: close\r\n\r\n",
                status.value, status.text(), content_type.as_cstr(), body_length as i64)
            var header_result = stream->write_all(&mut header, header_length as i64)
            if (header_result.is_error()) {
                return Result<i32, SocketError>::Error { error: header_result.unwrap_error() }
            }
            var body_result = stream->write_all(body.as_cstr(), body_length as i64)
            if (body_result.is_error()) {
                return Result<i32, SocketError>::Error { error: body_result.unwrap_error() }
            }
            return Result<i32, SocketError>::Ok { value: status.value }
        }
}
