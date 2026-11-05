import std.core.option.Option
import std.core.result.Result
import std.text.string.String
import std.text.view.StringView

public enum HttpParseError {
    Incomplete
    Malformed
    Unsupported
    TooLarge
}

public struct MessageFraming {
    public:
        static fn header_end(source: *StringView) -> Option<i64> {
            var bytes = source->as_ptr()
            for (var mut index: i64 = 0; index + 3 < source->length(); index = index + 1) {
                if (bytes[index] == '\r' && bytes[index + 1] == '\n' &&
                    bytes[index + 2] == '\r' && bytes[index + 3] == '\n') {
                    return Option<i64>::Some { value: index + 4 }
                }
            }
            return Option<i64>::None
        }

        static fn body_length(source: *StringView) -> Result<Option<i64>, HttpParseError> {
            var end = MessageFraming::header_end(source)
            if (end.is_none()) {
                if (source->length() > 65536) {
                    return Result<Option<i64>, HttpParseError>::Error { error: HttpParseError::TooLarge }
                }
                return Result<Option<i64>, HttpParseError>::Error { error: HttpParseError::Incomplete }
            }
            var header_end = end.unwrap()
            if (header_end > 65536) {
                return Result<Option<i64>, HttpParseError>::Error { error: HttpParseError::TooLarge }
            }
            var header_view = source->substring(0, header_end - 4).unwrap()
            var header = String(&header_view)
            var first_line = header.index_of("\r\n", 0)
            var mut line_start = header.length()
            if (first_line.is_some()) {
                line_start = first_line.unwrap() + 2
            }
            var mut length = Option<i64>::None
            while (line_start < header.length()) {
                var next_line = header.index_of("\r\n", line_start)
                var line_end = next_line.unwrap_or(header.length())
                var line = header.substring(line_start, line_end).unwrap()
                var normalized = line.to_lowercase()
                if (normalized.starts_with("transfer-encoding:")) {
                    return Result<Option<i64>, HttpParseError>::Error { error: HttpParseError::Unsupported }
                }
                if (normalized.starts_with("content-length:")) {
                    if (length.is_some()) {
                        return Result<Option<i64>, HttpParseError>::Error { error: HttpParseError::Malformed }
                    }
                    var raw_value = line.substring(15, line.length()).unwrap()
                    var trimmed = raw_value.trim()
                    for (var mut index: i64 = 0; index < trimmed.length(); index = index + 1) {
                        var character = trimmed.get(index).unwrap()
                        if ((character as i32) < ('0' as i32) || (character as i32) > ('9' as i32)) {
                            return Result<Option<i64>, HttpParseError>::Error { error: HttpParseError::Malformed }
                        }
                    }
                    var parsed = trimmed.parse_i64()
                    if (parsed.is_error() || parsed.unwrap() < 0) {
                        return Result<Option<i64>, HttpParseError>::Error { error: HttpParseError::Malformed }
                    }
                    if (parsed.unwrap() > 8388608) {
                        return Result<Option<i64>, HttpParseError>::Error { error: HttpParseError::TooLarge }
                    }
                    length = Option<i64>::Some { value: parsed.unwrap() }
                }
                if (!normalized.starts_with("content-length:") && normalized.index_of(":", 0).is_none()) {
                    return Result<Option<i64>, HttpParseError>::Error { error: HttpParseError::Malformed }
                }
                line_start = line_end + 2
            }
            return Result<Option<i64>, HttpParseError>::Ok { value: length }
        }
}
