import std.core.result.Result
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
import std.text.string.String

var REQUEST_READ_SIZE: i64 = 4095

@serializable
struct ErrorBody {
    public:
        var error: String
}

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

        static fn not_found(request: *Request, response: *mut Response) -> void {
            response->status(Status::NotFound)->text("not found")
        }

        fn dispatch(request: *Request, response: *mut Response) -> void {
            for (var mut index: i32 = 0; index < routes.size(); index = index + 1) {
                var route = routes.get(index).unwrap()
                if (route->matches(request)) {
                    var handler = route->handler
                    handler(request, response)
                    return
                }
            }

            var mut method_seen = false
            for (var mut index: i32 = 0; index < routes.size(); index = index + 1) {
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
                return Result<HttpServer, SocketError>::Error { error: SocketError::last() }
            }

            var listener = result.unwrap()
            var routes = Vector<Route>()

            return Result<HttpServer, SocketError>::Ok {
                value: HttpServer { listener: listener, routes: routes }
            }
        }

        fn local_port() -> u16 { return listener.local_port() }

        fn get(path: cstr, handler: Handler) mut -> void { self->add_route(Method::Get, path, handler) }
        fn post(path: cstr, handler: Handler) mut -> void { self->add_route(Method::Post, path, handler) }
        fn put(path: cstr, handler: Handler) mut -> void { self->add_route(Method::Put, path, handler) }
        fn delete(path: cstr, handler: Handler) mut -> void { self->add_route(Method::Delete, path, handler) }

        fn serve(on_start: () -> void) mut -> Result<i32, SocketError> {
            while (true) {
                on_start()

                var accept_result = listener.accept()
                if (accept_result.is_error()) {
                    return Result<i32, SocketError>::Error { error: SocketError::last() }
                }

                var mut stream = accept_result.unwrap()
                var mut buffer: u8[4096]
                var read_result = stream.read(&mut buffer, REQUEST_READ_SIZE)
                if (read_result.is_error()) {
                    stream.close()
                    return Result<i32, SocketError>::Error { error: SocketError::last() }
                }

                var length = read_result.unwrap() as i32
                var request = RequestParser::parse(&mut buffer, length)
                var mut response = Response { status: Status::Ok, content_type: String(), body: String() }
                self->dispatch(&request, &mut response)
                var response_result = response.send_to(&mut stream)

                stream.close()

                return response_result
            }
        }
}
