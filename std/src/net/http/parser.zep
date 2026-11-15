import std.text.string.String
import std.text.view.StringView
import std.core.result.Result
import std.core.option.Option
import std.net.http.framing.MessageFraming
import std.net.http.framing.HttpParseError
import std.net.http.types.Method
import std.net.http.types.Request

public struct RequestParser {
    public:
        static fn parse(source: &StringView) -> Result<Request, HttpParseError> {
            var length = when (MessageFraming::body_length(source)) {
                Result::Error { error } -> { return Result::Error { error: error } },
                Result::Ok { value } -> value.unwrap_or(0),
            }

            var body_start = MessageFraming::header_end(source).unwrap()
            var expected_length = body_start + length

            if (source.length() < expected_length) {
                return Result::Error { error: HttpParseError::Incomplete }
            }

            if (source.length() != expected_length) {
                return Result::Error { error: HttpParseError::Unsupported }
            }

            var method = Method::parse(source)

            if (method.value == Method::Unknown.value) {
                return Result::Error { error: HttpParseError::Unsupported }
            }

            var start = method.path_start() as i64
            var mut end = start
            var bytes = source.as_ptr()

            while (end < body_start && bytes[end] != ' ') {
                if ((bytes[end] as u8) < (33 as u8) || (bytes[end] as u8) == (127 as u8)) {
                    return Result::Error { error: HttpParseError::Malformed }
                }

                end = end + 1
            }

            if (end == start || end >= body_start || bytes[start] != '/') {
                return Result::Error { error: HttpParseError::Malformed }
            }

            var line_end = when (source.index_of("\r\n", end)) {
                Option::None -> { return Result::Error { error: HttpParseError::Malformed } },
                Option::Some { value } -> value,
            }

            var version = source.substring(end + 1, line_end).unwrap()

            if (!version.equals("HTTP/1.1") && !version.equals("HTTP/1.0")) {
                return Result::Error { error: HttpParseError::Unsupported }
            }

            return Result::Ok { value: Request {
                method: method,
                path: String(source.substring(start, end).unwrap()),
                body: String(source.substring(body_start, source.length()).unwrap())
            } }
        }

        static fn parse(source: &String) -> Result<Request, HttpParseError> {
            return RequestParser::parse(source.as_view())
        }

        static fn parse(buffer: *u8, length: i64) -> Result<Request, HttpParseError> {
            return when {
                (length < 0 || (buffer == null && length != 0)) ->
                    Result::Error { error: HttpParseError::Malformed },
                else -> RequestParser::parse(StringView(buffer as cstr, length)),
            }
        }
}
