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

        static fn body_length(source: &StringView) -> Result<Option<i64>, HttpParseError> {
            var header_end = when (MessageFraming::header_end(source)) {
                Option::Some { value } -> value,
                Option::None -> { return Result::Error { error: when {
                    (source.length() > 65536) -> HttpParseError::TooLarge,
                    else -> HttpParseError::Incomplete,
                } } },
            }

            if (header_end > 65536) {
                return Result::Error { error: HttpParseError::TooLarge }
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
                    return Result::Error { error: HttpParseError::Unsupported }
                }
                if (normalized.starts_with("content-length:")) {
                    if (length.is_some()) {
                        return Result::Error { error: HttpParseError::Malformed }
                    }
                    var trimmed = line.substring(15, line.length()).unwrap().trim()
                    for (var mut index: i64 = 0; index < trimmed.length(); index = index + 1) {
                        var character = trimmed.get(index).unwrap()
                        if (!Char::is_digit(character)) {
                            return Result::Error { error: HttpParseError::Malformed }
                        }
                    }
                    var parsed = when (trimmed.parse_i64()) {
                        Result::Error { error } -> { return Result::Error { error: HttpParseError::Malformed } },
                        Result::Ok { value } -> value,
                    }

                    if (parsed < 0) {
                        return Result::Error { error: HttpParseError::Malformed }
                    }
                    if (parsed > 8388608) {
                        return Result::Error { error: HttpParseError::TooLarge }
                    }
                    length = Option::Some { value: parsed }
                }
                if (!normalized.starts_with("content-length:") && normalized.index_of(":", 0).is_none()) {
                    return Result::Error { error: HttpParseError::Malformed }
                }
                line_start = line_end + 2
            }
            return Result::Ok { value: length }
        }
}
