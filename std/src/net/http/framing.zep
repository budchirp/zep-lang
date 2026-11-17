import std.core.option.Option
import std.core.error.Error
import std.text.string.String
import std.text.string.ParseError
import std.text.view.StringView

public enum HttpParseFailure : i32 {
    Incomplete = 0
    Malformed = 1
    Unsupported = 2
    TooLarge = 3
}

public struct HttpParseError : Error {
    public:
        var reason: HttpParseFailure
}

public struct MessageFraming {
    public:
        static fn header_end(source: &StringView) -> Option<i64> {
            var bytes = source.as_ptr()
            for (var mut index: i64 = 0; index + 3 < source.length(); index = index + 1) {
                if (bytes[index] == '\r' && bytes[index + 1] == '\n' &&
                    bytes[index + 2] == '\r' && bytes[index + 3] == '\n') {
                    return Option::Some { value: index + 4 }
                }
            }
            return Option::None
        }

        @throws(HttpParseError)
        static fn body_length(source: &StringView) -> Option<i64> {
            var header_end = when (MessageFraming::header_end(source)) {
                Option::Some { value } -> value,
                Option::None -> throw HttpParseError { reason: when {
                    (source.length() > 65536) -> HttpParseFailure::TooLarge,
                    else -> HttpParseFailure::Incomplete,
                } },
            }

            if (header_end > 65536) {
                throw HttpParseError { reason: HttpParseFailure::TooLarge }
            }
            var header = String(source.substring(0, header_end - 4).unwrap())
            var mut line_start = when (header.index_of("\r\n", 0)) {
                Option::None -> header.length(),
                Option::Some { value } -> value + 2,
            }
            var mut length: Option<i64> = Option::None
            while (line_start < header.length()) {
                var next_line = header.index_of("\r\n", line_start)
                var line_end = next_line.unwrap_or(header.length())
                var line = header.substring(line_start, line_end).unwrap()
                var normalized = line.to_lowercase()
                if (normalized.starts_with("transfer-encoding:")) {
                    throw HttpParseError { reason: HttpParseFailure::Unsupported }
                }
                if (normalized.starts_with("content-length:")) {
                    if (length.is_some()) {
                        throw HttpParseError { reason: HttpParseFailure::Malformed }
                    }
                    var trimmed = line.substring(15, line.length()).unwrap().trim()
                    for (var mut index: i64 = 0; index < trimmed.length(); index = index + 1) {
                        var character = trimmed.get(index).unwrap()
                        if (!Char::is_digit(character)) {
                            throw HttpParseError { reason: HttpParseFailure::Malformed }
                        }
                    }
                    var parsed = trimmed.parse_i64() catch {
                        error: ParseError -> throw HttpParseError { reason: HttpParseFailure::Malformed },
                    }

                    if (parsed < 0) {
                        throw HttpParseError { reason: HttpParseFailure::Malformed }
                    }
                    if (parsed > 8388608) {
                        throw HttpParseError { reason: HttpParseFailure::TooLarge }
                    }
                    length = Option::Some { value: parsed }
                }
                if (!normalized.starts_with("content-length:") && normalized.index_of(":", 0).is_none()) {
                    throw HttpParseError { reason: HttpParseFailure::Malformed }
                }
                line_start = line_end + 2
            }
            return length
        }
}
