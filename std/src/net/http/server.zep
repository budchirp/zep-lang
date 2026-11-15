import std.core.result.Result
import std.memory.Memory
import std.collections.vector.Vector
import std.net.socket.listener.TcpListener
import std.io.error.IoError
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
    Socket { error: IoError }
    Parse { error: HttpParseError }
}

struct Route {
    public:
        var method: Method
        var path: String
        var handler: Handler

        fn Route(method: Method, path: cstr, handler: Handler) -> Route {
            return Route { method: method, path: String(path), handler: handler }
        }

}

public struct HttpServer {
    private:
        var listener: TcpListener
        var routes: Vector<Route>

        fn dispatch(request: *Request, response: *mut Response) -> void {
            var mut method_seen = false

            for (var mut index: i64 = 0; index < routes.length(); index = index + 1) {
                var route = routes.get(index).unwrap()

                if (route->path.equals(request->path.as_cstr())) {
                    method_seen = true

                    if (route->method.matches(request->method)) {
                        route->handler(request, response)

                        return
                    }
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
        static fn bind(port: u16) -> Result<HttpServer, IoError> {
            return when (TcpListener::bind(Ipv4Address::localhost(), port)) {
                Result::Error { error } -> Result::Error { error: error },
                Result::Ok { value } -> Result::Ok {
                    value: HttpServer { listener: Memory::move(value), routes: Vector() }
                },
            }
        }

        fn local_port() -> Result<u16, IoError> {
            return listener.local_port()
        }

        fn get(path: cstr, handler: Handler) mut -> void {
            self->add_route(Method::Get, path, handler)
        }

        fn post(path: cstr, handler: Handler) mut -> void {
            self->add_route(Method::Post, path, handler)
        }

        fn put(path: cstr, handler: Handler) mut -> void {
            self->add_route(Method::Put, path, handler)
        }

        fn remove(path: cstr, handler: Handler) mut -> void {
            self->add_route(Method::Delete, path, handler)
        }

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
            var mut stream = when (listener.accept()) {
                Result::Error { error } -> { return Result::Error { error: HttpServerError::Socket { error: error } } },
                Result::Ok { value } -> Memory::move(value),
            }
            var mut incoming = String()
            var mut expected_length: i64 = -1

            while (expected_length < 0 || incoming.length() < expected_length) {
                var mut buffer: u8[4096]
                var count = when (stream.file.read(&mut buffer, #length(buffer))) {
                    Result::Error { error } -> { return Result::Error { error: HttpServerError::Socket { error: error } } },
                    Result::Ok { value } -> value,
                }

                if (count == 0) {
                    return Result::Error { error: HttpServerError::Parse { error: HttpParseError::Incomplete } }
                }

                if (incoming.length() + count > 8454144) {
                    return Result::Error { error: HttpServerError::Parse { error: HttpParseError::TooLarge } }
                }

                incoming.append(StringView((&buffer) as cstr, count))

                if (expected_length < 0) {
                    var view = incoming.as_view()
                    expected_length = when (MessageFraming::body_length(view)) {
                        Result::Ok { value } -> MessageFraming::header_end(view).unwrap() + value.unwrap_or(0),
                        Result::Error { error } -> when (error) {
                            HttpParseError::Incomplete -> -1,
                            else -> { return Result::Error { error: HttpServerError::Parse { error: error } } },
                        },
                    }
                }
            }

            var request = when (RequestParser::parse(incoming)) {
                Result::Error { error } -> { return Result::Error { error: HttpServerError::Parse { error: error } } },
                Result::Ok { value } -> Memory::move(value),
            }
            var mut response = Response { status: Status::Ok, content_type: String(), body: String() }
            self->dispatch(&request, &mut response)
            return when (response.send_to(&mut stream)) {
                Result::Error { error } -> Result::Error { error: HttpServerError::Socket { error: error } },
                Result::Ok { value } -> Result::Ok { value: value },
            }
        }
}
