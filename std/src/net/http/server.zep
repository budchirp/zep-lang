import std.core.result.Result
import std.memory.Memory
import std.collections.vector.Vector
import std.net.socket.listener.TcpListener
import std.net.socket.stream.TcpStream
import std.net.socket.error.SocketError
import std.net.socket.address.Ipv4Address
import std.net.http.types.Method
import std.net.http.types.Request
import std.net.http.types.Response
import std.net.http.types.Handler
import std.net.http.types.Status
import std.net.http.parser.RequestParser
import std.net.http.framing.MessageFraming
import std.net.http.framing.HttpParseError
import std.text.string.String
import std.text.view.StringView

public enum HttpServerError {
    Socket { error: SocketError }
    Parse { error: HttpParseError }
}

var REQUEST_READ_SIZE: i64 = 4095

struct Route {
    public:
        var method: Method
        var path: String
        var handler: Handler

        fn Route(method: Method, path: cstr, handler: Handler) -> Route {
            return Route { method: method, path: String(path), handler: handler }
        }

        fn matches(request: *Request) -> boolean {
            return path.equals(request->path.as_cstr()) && method.matches(request->method)
        }
}

public struct HttpServer {
    private:
        var listener: TcpListener
        var routes: Vector<Route>

        fn dispatch(request: *Request, response: *mut Response) -> void {
            for (var mut index: i64 = 0; index < routes.length(); index = index + 1) {
                var route = routes.get(index).unwrap()
                if (route->matches(request)) {
                    var handler = route->handler
                    handler(request, response)
                    return
                }
            }

            var mut method_seen = false
            for (var mut index: i64 = 0; index < routes.length(); index = index + 1) {
                var route = routes.get(index).unwrap()
                if (route->path.equals(request->path.as_cstr())) {
                    method_seen = true
                }
            }

            if (method_seen) {
                response->status(Status::MethodNotAllowed)->text("method not allowed")
            } else {
                response->status(Status::NotFound)->text("not found")
            }
        }

        fn add_route(method: Method, path: cstr, handler: Handler) mut -> void {
            routes.push(Route(method, path, handler))
        }

    public:
        static fn bind(port: u16) -> Result<HttpServer, SocketError> {
            var result = TcpListener::bind(Ipv4Address::localhost(), port)
            if (result.is_error()) {
                return Result<HttpServer, SocketError>::Error { error: result.unwrap_error() }
            }

            var listener = Memory::move(result).unwrap()
            var routes = Vector<Route>()

            return Result<HttpServer, SocketError>::Ok {
                value: HttpServer { listener: Memory::move(listener), routes: Memory::move(routes) }
            }
        }

        fn local_port() -> Result<u16, SocketError> { return listener.local_port() }

        fn get(path: cstr, handler: Handler) mut -> void { self->add_route(Method::Get, path, handler) }
        fn post(path: cstr, handler: Handler) mut -> void { self->add_route(Method::Post, path, handler) }
        fn put(path: cstr, handler: Handler) mut -> void { self->add_route(Method::Put, path, handler) }
        fn remove(path: cstr, handler: Handler) mut -> void { self->add_route(Method::Delete, path, handler) }

        fn serve(on_start: () -> void) mut -> Result<i32, HttpServerError> {
            on_start()
            while (true) {
                var result = self->serve_one()
                if (result.is_error()) {
                    return result
                }
            }
        }

        fn serve_one() mut -> Result<i32, HttpServerError> {
                var accept_result = listener.accept()
                if (accept_result.is_error()) {
                    return Result<i32, HttpServerError>::Error { error: HttpServerError::Socket { error: accept_result.unwrap_error() } }
                }

                var mut stream = Memory::move(accept_result).unwrap()
                var mut incoming = String()
                var mut expected_length: i64 = -1
                var mut complete = false
                while (!complete) {
                    var mut buffer: u8[4096]
                    var read_result = stream.read(&mut buffer, REQUEST_READ_SIZE)
                    if (read_result.is_error()) {
                        return Result<i32, HttpServerError>::Error { error: HttpServerError::Socket { error: read_result.unwrap_error() } }
                    }
                    var count = read_result.unwrap()
                    if (count == 0) {
                        return Result<i32, HttpServerError>::Error { error: HttpServerError::Parse { error: HttpParseError::Incomplete } }
                    }
                    if (incoming.length() + count > 8454144) {
                        return Result<i32, HttpServerError>::Error { error: HttpServerError::Parse { error: HttpParseError::TooLarge } }
                    }
                    var chunk = StringView((&buffer) as cstr, count)
                    incoming.append(&chunk)
                    if (expected_length < 0) {
                        var view = incoming.as_view()
                        var framing = MessageFraming::body_length(&view)
                        if (framing.is_ok()) {
                            expected_length = MessageFraming::header_end(&view).unwrap() + framing.unwrap().unwrap_or(0)
                        }
                        if (framing.is_error() && framing.unwrap_error().value != HttpParseError::Incomplete.value) {
                            return Result<i32, HttpServerError>::Error { error: HttpServerError::Parse { error: framing.unwrap_error() } }
                        }
                    }
                    complete = expected_length >= 0 && incoming.length() >= expected_length
                }

                var parsed = RequestParser::parse(incoming.as_cstr() as *u8, incoming.length())
                if (parsed.is_error()) {
                    return Result<i32, HttpServerError>::Error { error: HttpServerError::Parse { error: parsed.unwrap_error() } }
                }
                var request = Memory::move(parsed).unwrap()
                var mut response = Response { status: Status::Ok, content_type: String(), body: String() }
                self->dispatch(&request, &mut response)
                var response_result = response.send_to(&mut stream)

                stream.close()

                if (response_result.is_error()) {
                    return Result<i32, HttpServerError>::Error { error: HttpServerError::Socket { error: response_result.unwrap_error() } }
                }
                return Result<i32, HttpServerError>::Ok { value: response_result.unwrap() }
        }
}
