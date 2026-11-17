import database.Database
import std.core.Option
import std.net.http.Handler
import std.net.http.HttpServer
import std.net.http.Method
import std.net.http.Request
import std.net.http.Response
import std.net.http.Status
import std.text.String
import std.text.ParseError

public struct Api : Handler {
    private:
        var database: Database

        fn collection(request: *Request, response: *mut Response) mut -> void {
            if (request->method == Method::Get) {
                response->text(database.list())

                return
            }

            if (request->body.is_empty()) {
                response->status(Status::BadRequest)->text("note body is empty")

                return
            }

            response->status(Status::Created)->text(String::from(database.create(request->body)))
        }

        fn note(request: *Request, response: *mut Response) mut -> void {
            var id = request->path.substring(7, request->path.length()).unwrap().parse_i64() catch {
                error: ParseError -> {
                    response->status(Status::BadRequest)->text("invalid note id")

                    return
                },
            }

            if (id <= 0) {
                response->status(Status::BadRequest)->text("invalid note id")

                return
            }

            var status = when (request->method) {
                Method::Get -> when (database.get(id)) {
                    Option::Some { value } -> {
                        response->text(value->body)

                        Status::Ok
                    },
                    Option::None -> Status::NotFound,
                },
                Method::Put -> when {
                    (request->body.is_empty()) -> Status::BadRequest,
                    database.update(id, request->body) -> Status::Ok,
                    else -> Status::NotFound,
                },
                Method::Delete -> if (database.remove(id)) { Status::Ok } else { Status::NotFound },
                else -> Status::MethodNotAllowed,
            }

            response->status(status)

            if (status != Status::Ok || request->method != Method::Get) {
                response->text(status.text())
            }
        }

    public:
        fn Api() -> Api {
            return Api { database: Database() }
        }

        fn register(server: *mut HttpServer) mut -> void {
            server->handle(Method::Get, "/notes", *self)
            server->handle(Method::Post, "/notes", *self)
            server->handle(Method::Get, "/notes/*", *self)
            server->handle(Method::Put, "/notes/*", *self)
            server->handle(Method::Delete, "/notes/*", *self)
        }

        override fn handle(request: *Request, response: *mut Response) mut -> void {
            if (request->path.equals("/notes")) {
                self->collection(request, response)
            } else {
                self->note(request, response)
            }
        }
}
