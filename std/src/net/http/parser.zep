import std.text.string.String
import std.net.http.types.Method
import std.net.http.types.Request

var REQUEST_BUFFER_SIZE: i32 = 4096

public struct RequestParser {
    private:
        var buffer: *mut u8
        var length: i32

        fn find_path_end(start: i32) -> i32 {
            var mut index = start

            while (index < length && buffer[index] != ' ' as u8) {
                index = index + 1
            }

            return index
        }

        fn find_body_start() -> i32 {
            if (length < 4) {
                return length
            }

            for (var mut index: i32 = 0; index < length - 3; index = index + 1) {
                if (buffer[index] == '\r' as u8 &&
                    buffer[index + 1] == '\n' as u8 &&
                    buffer[index + 2] == '\r' as u8 &&
                    buffer[index + 3] == '\n' as u8) {
                    return index + 4
                }
            }

            return length
        }

    public:
        static fn parse(buffer: *mut u8, length: i32) -> Request {
            var parser = RequestParser { buffer: buffer, length: length }
            var bytes = buffer
            var method = Method::parse(bytes as cstr)
            var start = method.path_start()
            var end = parser.find_path_end(start)
            var body_start = parser.find_body_start()

            var mut path = String()
            for (var mut index: i32 = start; index < end; index = index + 1) {
                path.push(bytes[index] as char)
            }

            var mut body = String()
            for (var mut index: i32 = body_start; index < length; index = index + 1) {
                body.push(bytes[index] as char)
            }

            return Request {
                method: method,
                path: path,
                body: body
            }
        }
}
