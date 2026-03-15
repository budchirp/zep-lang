extern fn printf(fmt: string, ...args: string[]): void

private extern fn malloc(size: i64): *void
extern fn free(ptr: *void): void

private fn allocate<T>(value: T): *mut T {
    var ptr = malloc(123 as i64) as *mut T;
    *ptr = value

    return ptr
}

public struct Error {
    public message: string
}

struct Result<T> {
    value: T
    error: Error
}

fn add<T: i32>(a: T, b: T): Result<T> {
    return Result<T> {
        value: a + b,
        error: Error {
            message: ""
        }
    }
}

struct Vector {
    x: i32
    y: i32
    z: i32
}

fn main(): i32 {
    var vector_ptr = allocate<Vector>(Vector {
        x: 10,
        y: 20,
        z: 30
    })

    var vector = *vector_ptr

    var result = add<i32>(5, 20) 
    if (result.error.message != "") {
        printf("error: %s\n", result.error.message)
        return 1
    }

    printf("`add`: %d\n", result.value as string)

    printf("`vector`: %d, %d, %d\n", vector.x as string, vector.y as string, vector.z as string)

    free(vector_ptr)

    return 0
}