import handlers.health

type Handler = (i32) -> i32

fn apply(handler: Handler) -> i32 {
    return handler(41)
}

public fn main() -> i32 {
    var handler: Handler = health

    return apply(handler) - 42
}
