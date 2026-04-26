struct Result<T> {
    value: T
    error: Error
}

struct Error {
    message: string
}

extern fn printf(fmt: string, ...args: string[]): void

fn add<T>(a: T, b: T): Result<T> {
    return Result<T> {
        value: a,
        error: Error { message: "" }
    }
}

fn main(): i32 {
    var res1: Result<i32> = add<i32>(1, 2)
    var res2: Result<f32> = add<f32>(1.0 as f32, 2.0 as f32)
    return 0
}
