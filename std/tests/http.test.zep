import std.net.http.framing.MessageFraming
import std.net.http.framing.HttpParseError
import std.net.http.parser.RequestParser
import std.text.StringView
import std.testing.Test

test "HTTP parsing rejects incomplete and unsupported framing" {
    var parsed = RequestParser::parse(StringView::from_cstr("GET /test HTTP/1.1\r\nHost: localhost\r\n\r\n"))

    Test::expect(parsed.is_ok() && parsed.unwrap().path.equals("/test"))

    var incomplete = MessageFraming::body_length(StringView::from_cstr("GET / HTTP/1.1\r\n"))

    Test::expect(incomplete.is_error() && incomplete.unwrap_error().value == HttpParseError::Incomplete.value)
    Test::expect(MessageFraming::body_length(StringView::from_cstr(
        "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n")).is_error())
}
