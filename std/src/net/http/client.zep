import std.core.result.Result
import std.memory.Memory
import std.text.string.String
import std.text.view.StringView
import std.net.socket.stream.TcpStream
import std.net.socket.address.Ipv4Address
import std.net.http.framing.MessageFraming

public enum HttpError {
    ConnectFailed
    SendFailed
    RecvFailed
    ParseFailed
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
        static fn request(address: Ipv4Address, method: cstr, host: cstr, port: u16, path: cstr,
                         body: cstr, content_type: cstr) -> Result<HttpResponse, HttpError> {
            var stream = when (TcpStream::connect(address, port)) {
                Result::Error { error } -> { return Result::Error { error: HttpError::ConnectFailed } },
                Result::Ok { value } -> Memory::move(value),
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

            if (stream.file.write(request).is_error()) {
                return Result::Error { error: HttpError::SendFailed }
            }

            var mut response = String()
            var mut reading = true
            while (reading) {
                var mut buffer: u8[4096]
                var read_length = when (stream.file.read(&mut buffer, #length(buffer))) {
                    Result::Error { error } -> { return Result::Error { error: HttpError::RecvFailed } },
                    Result::Ok { value } -> value,
                }
                reading = read_length != 0

                if (reading) {
                    if (response.length() + read_length > 8454144) {
                        return Result::Error { error: HttpError::ParseFailed }
                    }

                    response.append(StringView((&buffer) as cstr, read_length))
                }
            }

            var response_length = response.length()
            if (response_length < 13 ||
                (!response.starts_with("HTTP/1.1 ") && !response.starts_with("HTTP/1.0 "))) {
                return Result::Error { error: HttpError::ParseFailed }
            }

            var bytes = response.as_cstr()
            var view = response.as_view()
            var declared_length = when (MessageFraming::body_length(view)) {
                Result::Error { error } -> { return Result::Error { error: HttpError::ParseFailed } },
                Result::Ok { value } -> value,
            }

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
                return Result::Error { error: HttpError::ParseFailed }
            }

            var body_start = MessageFraming::header_end(view).unwrap()
            if (declared_length.is_some() && body_start + declared_length.unwrap() != response_length) {
                return Result::Error { error: HttpError::ParseFailed }
            }

            return Result::Ok { value: HttpResponse(status,
                String(StringView((&bytes[body_start]) as cstr, response_length - body_start))) }
        }

        static fn get(address: Ipv4Address, host: cstr, port: u16, path: cstr) -> Result<HttpResponse, HttpError> {
            return HttpClient::request(address, "GET", host, port, path, "", "")
        }

        static fn post(address: Ipv4Address, host: cstr, port: u16, path: cstr,
                       body: cstr, content_type: cstr) -> Result<HttpResponse, HttpError> {
            return HttpClient::request(address, "POST", host, port, path, body, content_type)
        }

        static fn put(address: Ipv4Address, host: cstr, port: u16, path: cstr,
                      body: cstr, content_type: cstr) -> Result<HttpResponse, HttpError> {
            return HttpClient::request(address, "PUT", host, port, path, body, content_type)
        }

        static fn remove(address: Ipv4Address, host: cstr, port: u16, path: cstr) -> Result<HttpResponse, HttpError> {
            return HttpClient::request(address, "DELETE", host, port, path, "", "")
        }
}
