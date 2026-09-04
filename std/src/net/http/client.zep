import std.core.result.Result
import std.text.string.String
import std.net.socket.stream.TcpStream
import std.net.socket.address.Ipv4Address

public enum HttpError {
    InvalidUrl
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
            return HttpResponse { status: status, body: body }
        }

        fn is_success() -> boolean {
            return status >= 200 && status < 300
        }
}

public struct HttpClient {
    public:
        static fn request(method: cstr, host: cstr, port: u16, path: cstr,
                         body: cstr, content_type: cstr) -> Result<HttpResponse, HttpError> {
            var connect_result = TcpStream::connect(Ipv4Address::localhost(), port)
            if (connect_result.is_error()) {
                return Result<HttpResponse, HttpError>::Error { error: HttpError::ConnectFailed }
            }

            var mut stream = connect_result.unwrap()

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
                var mut length_text = String::from(body_length)
                request.append(&length_text)
                request.append("\r\n\r\n")
                request.append(body)
            } else {
                request.append("\r\n")
            }

            var write_result = stream.write_all(request.as_cstr(), request.length() as i64)
            if (write_result.is_error()) {
                stream.close()
                return Result<HttpResponse, HttpError>::Error { error: HttpError::SendFailed }
            }

            var mut response = String()
            var mut reading = true
            while (reading) {
                var mut buffer: u8[4096]
                var read_result = stream.read(&mut buffer, 4095)
                if (read_result.is_error()) {
                    stream.close()
                    return Result<HttpResponse, HttpError>::Error { error: HttpError::RecvFailed }
                }

                var read_length = read_result.unwrap() as i32
                if (read_length == 0) {
                    reading = false
                } else {
                    buffer[read_length] = 0 as u8
                    response.append((&mut buffer) as cstr)
                }
            }
            stream.close()

            var response_length = response.length()
            if (response_length < 12) {
                return Result<HttpResponse, HttpError>::Error { error: HttpError::ParseFailed }
            }

            var bytes = response.as_cstr()

            var mut status_start: i32 = 9
            var mut status = 0
            while (status_start < response_length &&
                   (bytes[status_start] as i32) >= ('0' as i32) &&
                   (bytes[status_start] as i32) <= ('9' as i32)) {
                var digit = bytes[status_start] as i32
                status = (status * 10) + (digit - ('0' as i32))
                status_start = status_start + 1
            }

            var mut body_start = response_length
            for (var mut index: i32 = 0; index < response_length - 3; index = index + 1) {
                if (bytes[index] == '\r' &&
                    bytes[index + 1] == '\n' &&
                    bytes[index + 2] == '\r' &&
                    bytes[index + 3] == '\n') {
                    body_start = index + 4
                }
            }

            var mut response_body = String()
            if (body_start < response_length) {
                for (var mut index: i32 = body_start; index < response_length; index = index + 1) {
                    response_body.push(bytes[index])
                }
            }

            return Result<HttpResponse, HttpError>::Ok { value: HttpResponse(status, response_body) }
        }

        static fn get(host: cstr, port: u16, path: cstr) -> Result<HttpResponse, HttpError> {
            return HttpClient::request("GET", host, port, path, "", "")
        }

        static fn post(host: cstr, port: u16, path: cstr,
                       body: cstr, content_type: cstr) -> Result<HttpResponse, HttpError> {
            return HttpClient::request("POST", host, port, path, body, content_type)
        }

        static fn put(host: cstr, port: u16, path: cstr,
                      body: cstr, content_type: cstr) -> Result<HttpResponse, HttpError> {
            return HttpClient::request("PUT", host, port, path, body, content_type)
        }

        static fn delete(host: cstr, port: u16, path: cstr) -> Result<HttpResponse, HttpError> {
            return HttpClient::request("DELETE", host, port, path, "", "")
        }
}
