import std.net.http.Method
import std.net.http.Request
import std.net.http.Response
import std.net.http.Status
import std.text.String
import std.testing.Test

test "API creates reads updates and deletes notes" {
    var mut api = Api()
    var mut response = Response()
    var mut request = Request { method: Method::Post, path: String("/notes"), body: String("hello") }
    api.handle(&request, &mut response)

    Test::expect(response.status == Status::Created && response.body.equals("1"))

    request = Request { method: Method::Get, path: String("/notes/1"), body: String() }
    api.handle(&request, &mut response)

    Test::expect(response.status == Status::Ok && response.body.equals("hello"))

    request.method = Method::Put
    request.body = String("updated")
    api.handle(&request, &mut response)
    request.method = Method::Get
    api.handle(&request, &mut response)

    Test::expect(response.body.equals("updated"))

    request.method = Method::Delete
    api.handle(&request, &mut response)
    request.method = Method::Get
    api.handle(&request, &mut response)

    Test::expect(response.status == Status::NotFound)
}

test "API rejects empty notes and invalid IDs" {
    var mut api = Api()
    var mut response = Response()
    var mut request = Request { method: Method::Post, path: String("/notes"), body: String() }
    api.handle(&request, &mut response)

    Test::expect(response.status == Status::BadRequest)

    request.method = Method::Get
    request.path = String("/notes/not-an-id")
    api.handle(&request, &mut response)

    Test::expect(response.status == Status::BadRequest)
}
