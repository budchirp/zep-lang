import std.text.string.String
import std.text.view.StringView
import std.net.socket.stream.TcpStream
import std.io.error.IoError
import std.core.result.Result

public enum Method {
    Get
    Post
    Put
    Delete
    Unknown

    public:
        static fn parse(buffer: &StringView) -> Method {
            return when {
                buffer.starts_with("GET ") -> Method::Get,
                buffer.starts_with("POST ") -> Method::Post,
                buffer.starts_with("PUT ") -> Method::Put,
                buffer.starts_with("DELETE ") -> Method::Delete,
                else -> Method::Unknown,
            }
        }

        fn matches(other: Method) -> boolean {
            return (*self).value == other.value
        }

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

        fn send_to(stream: *mut TcpStream) -> Result<i32, IoError> {
            var mut header = String("HTTP/1.1 ")
            header.append(String::from(status.value))
            header.append(" ")
            header.append(status.text())
            header.append("\r\nContent-Type: ")
            header.append(content_type)
            header.append("\r\nContent-Length: ")
            header.append(String::from(body.length()))
            header.append("\r\nConnection: close\r\n\r\n")
            when (stream->file.write(header)) {
                Result::Error { error } -> { return Result::Error { error: error } },
                Result::Ok { value } -> {},
            }

            return when (stream->file.write(body)) {
                Result::Error { error } -> Result::Error { error: error },
                Result::Ok { value } -> Result::Ok { value: status.value },
            }
        }
}
