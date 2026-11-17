import std.text.string.String
import std.text.view.StringView
import std.net.socket.stream.TcpStream
import std.io.error.IoError

public enum Method {
    Get
    Post
    Put
    Delete
    Unknown

    public:
        fn text() -> cstr {
            return when (*self) {
                Method::Get -> "GET",
                Method::Post -> "POST",
                Method::Put -> "PUT",
                Method::Delete -> "DELETE",
                Method::Unknown -> "",
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

public interface Handler {
    public:
        fn handle(request: *Request, response: *mut Response) mut -> void
}

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

        fn Response() -> Response {
            return Response { status: Status::Ok, content_type: String("text/plain"), body: String() }
        }

        fn status(code: Status) mut -> *mut Response {
            self->status = code
            return self
        }

        fn text(body: cstr) mut -> *mut Response {
            return self->text(StringView::from_cstr(body))
        }

        fn text(body: &String) mut -> *mut Response {
            return self->text(body.as_view())
        }

        fn text(body: &StringView) mut -> *mut Response {
            content_type = String("text/plain")
            self->body = String(body)

            return self
        }

        @throws(IoError)
        fn send_to(stream: *mut TcpStream) -> i32 {
            var mut header = String("HTTP/1.1 ")
            header.append(String::from(status.value))
            header.append(" ")
            header.append(status.text())
            header.append("\r\nContent-Type: ")
            header.append(content_type)
            header.append("\r\nContent-Length: ")
            header.append(String::from(body.length()))
            header.append("\r\nConnection: close\r\n\r\n")
            try stream->file.write(header)
            try stream->file.write(body)

            return status.value
        }
}
