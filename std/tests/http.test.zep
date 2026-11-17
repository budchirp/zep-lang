import std.net.http.framing.MessageFraming
import std.net.http.framing.HttpParseError
import std.net.http.framing.HttpParseFailure
import std.net.http.parser.RequestParser
import std.net.http.Method
import std.net.http.Response
import std.net.http.Status
import std.net.http.HttpClient
import std.net.http.HttpError
import std.net.socket.Ipv4Address
import std.text.String
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

test "HTTP method names round trip and responses own binary text" {
    var methods = [Method::Get, Method::Post, Method::Put, Method::Delete]

    for (var mut index: i64 = 0; index < #length(methods); index = index + 1) {
        var mut source = String(methods[index].text())
        source.append(" /notes HTTP/1.1\r\nContent-Length: 0\r\n\r\n")
        var request = RequestParser::parse(source) catch {
            error: HttpParseError -> Test::fail("supported method failed to parse"),
        }

        Test::expect(request.method == methods[index])
    }

    var mut bytes: char[3] = ['a', 0 as char, 'b']
    var mut response = Response()
    response.text(StringView((&bytes) as cstr, #length(bytes)))
    bytes[0] = 'x'

    Test::expect(response.status == Status::Ok)
    Test::expect(response.body.length() == 3 && response.body.get(0).unwrap() == 'a')
    Test::expect(response.body.get(1).unwrap() == 0 as char)
}

test "HTTP client rejects unknown methods and injected request lines before connecting" {
    var client = HttpClient(Ipv4Address::localhost(), "localhost", 0 as u16)

    try {
        client.handle(Method::Unknown, "/")
        Test::fail("unknown method was accepted")
    } catch {
        error: HttpParseError -> Test::expect(error.reason == HttpParseFailure::Unsupported),
        error: HttpError -> Test::fail("validation attempted a connection"),
    }

    try {
        client.handle(Method::Get, "/\r\nInjected: value")
        Test::fail("request injection was accepted")
    } catch {
        error: HttpParseError -> Test::expect(error.reason == HttpParseFailure::Malformed),
        error: HttpError -> Test::fail("validation attempted a connection"),
    }
}
