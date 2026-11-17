public struct Config {
    public:
        var port: i32
}

public fn handle_request(config: Config) -> i32 {
    return config.port
}
