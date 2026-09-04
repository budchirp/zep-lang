import handlers.Config
import handlers.handle_request

public fn main() -> i32 {
    var config = Config { port: 8080 }
    return handle_request(config)
}
