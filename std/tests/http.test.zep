import std.net.http.framing.MessageFraming
import std.net.http.framing.HttpParseError
import std.net.http.framing.HttpParseFailure
import std.net.http.parser.RequestParser
import std.text.StringView
import std.testing.Test

test "HTTP parsing rejects incomplete and unsupported framing" {
    var parsed = RequestParser::parse(StringView::from_cstr("GET /test HTTP/1.1\r\nHost: localhost\r\n\r\n")) catch {
        error: HttpParseError -> Test::fail("valid HTTP request was rejected"),
    }

    Test::expect(parsed.path.equals("/test"))

    try {
        MessageFraming::body_length(StringView::from_cstr("GET / HTTP/1.1\r\n"))
        Test::fail("incomplete HTTP request was accepted")
    } catch { error: HttpParseError -> { Test::expect(error.reason == HttpParseFailure::Incomplete) }, }

    try {
        MessageFraming::body_length(StringView::from_cstr("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"))
        Test::fail("chunked HTTP response was accepted")
    } catch { error: HttpParseError -> { Test::expect(error.reason == HttpParseFailure::Unsupported) }, }
}
