import std.core.error.Error
import std.io.error.IoError
import std.memory.Memory
import std.text.string.String
import std.text.view.StringView
import std.net.socket.stream.TcpStream
import std.net.socket.address.Ipv4Address
import std.net.http.framing.MessageFraming
import std.net.http.framing.HttpParseError
import std.net.http.framing.HttpParseFailure

public enum HttpOperation : i32 {
    Connect = 0
    Send = 1
    Receive = 2
}

public struct HttpError : Error {
    public:
        var operation: HttpOperation
        var error: IoError
}

public struct HttpResponse {
    public:
        var status: i32
        var body: String

        fn HttpResponse(status: i32, body: String) -> HttpResponse {
            return HttpResponse { status: status, body: Memory::move(body) }
        }

        fn is_success() -> boolean {
            return status >= 200 && status < 300
        }
}

public struct HttpClient {
    public:
        @throws(HttpError, HttpParseError)
        static fn request(address: Ipv4Address, method: cstr, host: cstr, port: u16, path: cstr,
                         body: cstr, content_type: cstr) -> HttpResponse {
            var stream = TcpStream::connect(address, port) catch {
                error: IoError -> throw HttpError { operation: HttpOperation::Connect, error: error },
            }

            var mut request = String(method)
            request.append(" ")
            request.append(path)
            request.append(" HTTP/1.1\r\nHost: ")
            request.append(host)
            request.append("\r\nConnection: close\r\n")

            var body_length = body.length()
            if (body_length > 0) {
                request.append("Content-Type: ")
                request.append(content_type)
                request.append("\r\nContent-Length: ")
                request.append(String::from(body_length))
                request.append("\r\n\r\n")
                request.append(body)
            } else {
                request.append("\r\n")
            }

            stream.file.write(request) catch {
                error: IoError -> throw HttpError { operation: HttpOperation::Send, error: error },
            }

            var mut response = String()
            var mut reading = true
            while (reading) {
                var mut buffer: u8[4096]
                var read_length = stream.file.read(&mut buffer, #length(buffer)) catch {
                    error: IoError -> throw HttpError { operation: HttpOperation::Receive, error: error },
                }
                reading = read_length != 0

                if (reading) {
                    if (response.length() + read_length > 8454144) {
                        throw HttpParseError { reason: HttpParseFailure::TooLarge }
                    }

                    response.append(StringView((&buffer) as cstr, read_length))
                }
            }

            var response_length = response.length()
            if (response_length < 13 ||
                (!response.starts_with("HTTP/1.1 ") && !response.starts_with("HTTP/1.0 "))) {
                throw HttpParseError { reason: HttpParseFailure::Malformed }
            }

            var bytes = response.as_cstr()
            var view = response.as_view()
            var declared_length = try MessageFraming::body_length(view)

            var mut status_start: i64 = 9
            var mut status = 0
            while (status_start < 12 &&
                   (bytes[status_start] as i32) >= ('0' as i32) &&
                   (bytes[status_start] as i32) <= ('9' as i32)) {
                var digit = bytes[status_start] as i32
                status = (status * 10) + (digit - ('0' as i32))
                status_start = status_start + 1
            }
            if (status_start != 12 || bytes[12] != ' ' || status < 100) {
                throw HttpParseError { reason: HttpParseFailure::Malformed }
            }

            var body_start = MessageFraming::header_end(view).unwrap()
            if (declared_length.is_some() && body_start + declared_length.unwrap() != response_length) {
                throw HttpParseError { reason: HttpParseFailure::Malformed }
            }

            return HttpResponse(status,
                String(StringView((&bytes[body_start]) as cstr, response_length - body_start)))
        }

        @throws(HttpError, HttpParseError)
        static fn get(address: Ipv4Address, host: cstr, port: u16, path: cstr) -> HttpResponse {
            return try HttpClient::request(address, "GET", host, port, path, "", "")
        }

        @throws(HttpError, HttpParseError)
        static fn post(address: Ipv4Address, host: cstr, port: u16, path: cstr,
                       body: cstr, content_type: cstr) -> HttpResponse {
            return try HttpClient::request(address, "POST", host, port, path, body, content_type)
        }

        @throws(HttpError, HttpParseError)
        static fn put(address: Ipv4Address, host: cstr, port: u16, path: cstr,
                      body: cstr, content_type: cstr) -> HttpResponse {
            return try HttpClient::request(address, "PUT", host, port, path, body, content_type)
        }

        @throws(HttpError, HttpParseError)
        static fn remove(address: Ipv4Address, host: cstr, port: u16, path: cstr) -> HttpResponse {
            return try HttpClient::request(address, "DELETE", host, port, path, "", "")
        }
}
