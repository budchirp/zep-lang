import std.text.string.String
import std.text.view.StringView
import std.core.result.Result
import std.net.http.framing.MessageFraming
import std.net.http.framing.HttpParseError
import std.net.http.types.Method
import std.net.http.types.Request

public struct RequestParser {
    private:
        var buffer: *u8
        var length: i64

        fn find_path_end(start: i64) -> i64 {
            var mut index = start

            while (index < length && buffer[index] != ' ' as u8) {
                index = index + 1
            }

            return index
        }

    public:
        static fn parse(buffer: *u8, length: i64) -> Result<Request, HttpParseError> {
            if (length < 0 || (buffer == null && length != 0)) {
                return Result<Request, HttpParseError>::Error { error: HttpParseError::Malformed }
            }
            var view = StringView(buffer as cstr, length)
            var framing = MessageFraming::body_length(&view)
            if (framing.is_error()) {
                return Result<Request, HttpParseError>::Error { error: framing.unwrap_error() }
            }
            var body_start = MessageFraming::header_end(&view).unwrap()
            var expected_length = body_start + framing.unwrap().unwrap_or(0)
            if (length < expected_length) {
                return Result<Request, HttpParseError>::Error { error: HttpParseError::Incomplete }
            }
            if (length != expected_length) {
                return Result<Request, HttpParseError>::Error { error: HttpParseError::Unsupported }
            }
            var parser = RequestParser { buffer: buffer, length: length }
            var bytes = buffer
            var bounded = String(&view)
            var method = Method::parse(bounded.as_cstr())
            if (method.value == Method::Unknown.value) {
                return Result<Request, HttpParseError>::Error { error: HttpParseError::Unsupported }
            }
            var start = method.path_start() as i64
            var end = parser.find_path_end(start)
            if (end == start || end >= body_start) {
                return Result<Request, HttpParseError>::Error { error: HttpParseError::Malformed }
            }

            var line_end = bounded.index_of("\r\n", end)
            if (line_end.is_none() || bytes[start] != '/' as u8) {
                return Result<Request, HttpParseError>::Error { error: HttpParseError::Malformed }
            }
            var version = bounded.substring(end + 1, line_end.unwrap()).unwrap()
            if (!version.equals("HTTP/1.1") && !version.equals("HTTP/1.0")) {
                return Result<Request, HttpParseError>::Error { error: HttpParseError::Unsupported }
            }

            var mut path = String()
            for (var mut index: i64 = start; index < end; index = index + 1) {
                if (bytes[index] < 33 as u8 || bytes[index] == 127 as u8) {
                    return Result<Request, HttpParseError>::Error { error: HttpParseError::Malformed }
                }
                path.push(bytes[index] as char)
            }

            var mut body = String()
            for (var mut index: i64 = body_start; index < length; index = index + 1) {
                body.push(bytes[index] as char)
            }

            return Result<Request, HttpParseError>::Ok { value: Request {
                method: method,
                path: path,
                body: body
            } }
        }
}
