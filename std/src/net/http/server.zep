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
import std.net.http.framing.HttpParseFailure
import std.text.string.String
import std.text.view.StringView

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
        @throws(IoError)
        static fn bind(port: u16) -> HttpServer {
            return HttpServer { listener: try TcpListener::bind(Ipv4Address::localhost(), port), routes: Vector() }
        }

        @throws(IoError)
        fn local_port() -> u16 {
            return try listener.local_port()
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

        @throws(IoError, HttpParseError)
        fn serve(on_start: () -> void) mut -> never {
            on_start()

            while (true) {
                try self->serve_one()
            }
        }

        @throws(IoError, HttpParseError)
        fn serve_one() mut -> i32 {
            var mut stream = try listener.accept()
            var mut incoming = String()
            var mut expected_length: i64 = -1

            while (expected_length < 0 || incoming.length() < expected_length) {
                var mut buffer: u8[4096]
                var count = try stream.file.read(&mut buffer, #length(buffer))

                if (count == 0) {
                    throw HttpParseError { reason: HttpParseFailure::Incomplete }
                }

                if (incoming.length() + count > 8454144) {
                    throw HttpParseError { reason: HttpParseFailure::TooLarge }
                }

                incoming.append(StringView((&buffer) as cstr, count))

                if (expected_length < 0) {
                    var view = incoming.as_view()
                    expected_length = try {
                        var length = MessageFraming::body_length(view)

                        MessageFraming::header_end(view).unwrap() + length.unwrap_or(0)
                    } catch {
                        error: HttpParseError -> when (error.reason) {
                            HttpParseFailure::Incomplete -> -1 as i64,
                            else -> throw error,
                        },
                    }
                }
            }

            var request = try RequestParser::parse(incoming)
            var mut response = Response { status: Status::Ok, content_type: String(), body: String() }
            self->dispatch(&request, &mut response)

            return try response.send_to(&mut stream)
        }
}
