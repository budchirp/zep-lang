import std.text.string.String
import std.text.view.StringView
import std.core.option.Option
import std.net.http.framing.MessageFraming
import std.net.http.framing.HttpParseError
import std.net.http.framing.HttpParseFailure
import std.net.http.types.Method
import std.net.http.types.Request

public struct RequestParser {
    public:
        @throws(HttpParseError)
        static fn parse(source: &StringView) -> Request {
            var length = (try MessageFraming::body_length(source)).unwrap_or(0)

            var body_start = MessageFraming::header_end(source).unwrap()
            var expected_length = body_start + length

            if (source.length() < expected_length) {
                throw HttpParseError { reason: HttpParseFailure::Incomplete }
            }

            if (source.length() != expected_length) {
                throw HttpParseError { reason: HttpParseFailure::Unsupported }
            }

            var start = when (source.index_of(" ", 0)) {
                Option::Some { value } -> value,
                Option::None -> throw HttpParseError { reason: HttpParseFailure::Malformed },
            }
            var name = source.substring(0, start).unwrap()
            var method = when {
                name.equals(Method::Get.text()) -> Method::Get,
                name.equals(Method::Post.text()) -> Method::Post,
                name.equals(Method::Put.text()) -> Method::Put,
                name.equals(Method::Delete.text()) -> Method::Delete,
                else -> Method::Unknown,
            }

            if (method.value == Method::Unknown.value) {
                throw HttpParseError { reason: HttpParseFailure::Unsupported }
            }

            var mut end = start + 1
            var bytes = source.as_ptr()

            while (end < body_start && bytes[end] != ' ') {
                if ((bytes[end] as u8) < (33 as u8) || (bytes[end] as u8) == (127 as u8)) {
                    throw HttpParseError { reason: HttpParseFailure::Malformed }
                }

                end = end + 1
            }

            if (end == start + 1 || end >= body_start || bytes[start + 1] != '/') {
                throw HttpParseError { reason: HttpParseFailure::Malformed }
            }

            var line_end = when (source.index_of("\r\n", end)) {
                Option::None -> throw HttpParseError { reason: HttpParseFailure::Malformed },
                Option::Some { value } -> value,
            }

            var version = source.substring(end + 1, line_end).unwrap()

            if (!version.equals("HTTP/1.1") && !version.equals("HTTP/1.0")) {
                throw HttpParseError { reason: HttpParseFailure::Unsupported }
            }

            return Request {
                method: method,
                path: String(source.substring(start + 1, end).unwrap()),
                body: String(source.substring(body_start, source.length()).unwrap())
            }
        }

        @throws(HttpParseError)
        static fn parse(source: &String) -> Request {
            return try RequestParser::parse(source.as_view())
        }

        @throws(HttpParseError)
        static fn parse(buffer: *u8, length: i64) -> Request {
            return when {
                (length < 0 || (buffer == null && length != 0)) ->
                    throw HttpParseError { reason: HttpParseFailure::Malformed },
                else -> try RequestParser::parse(StringView(buffer as cstr, length)),
            }
        }
}
