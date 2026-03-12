struct Point {
    x: i32
    y: i32
}

struct Color {
    private r: i32
    private g: i32
    private b: i32
}

private struct Secret {
    value: string
}

fn add(a: i32, b: i32): i32 {
    return a + b
}

private fn negate(x: i32): i32 {
    return -x
}

fn create_point(x: i32, y: i32): Point {
    return Point {
        x: x,
        y: y
    }
}

fn distance_squared(p: Point): i32 {
    return p.x * p.x + p.y * p.y
}

fn sum_array(arr: i32[], size: i32): i32 {
    var total = 0
    return total
}

fn use_matrix(matrix: f64[3][3]): void {
    return
}

fn swap(ptr: *i32, other: *i32): void {
    var temp = *ptr;
    *ptr = *other;
    *other = temp;
}

fn main(): i32 {
    var x = 10
    var y: i32 = 20
    const pi = 3.14
    var mut counter = 0

    var sum = add(x, y)

    var point = create_point(1, 2)
    var dist = distance_squared(point)

    var name = "hello"
    var flag = true
    var negated = !flag

    var a = 5
    var b = 10
    swap(&a, &b)

    var result = a + b * 2 - 1
    var quotient = 100 / 3
    var remainder = 100 % 3

    var eq = a == b
    var neq = a != b
    var lt = a < b
    var gt = a > b
    var lte = a <= b
    var gte = a >= b

    var both = flag && true
    var either = flag || false

    var color = Color {
        r: 255,
        g: 128,
        b: 0
    }

    counter = counter + 1

    if (sum > 20) {
        return 1
    } else if (sum == 20) {
        return 2
    } else {
        return 0
    }

    var cast = x as f64

    if (flag is bool) {
        return 42
    }

    var positive = +x
    var negative = -x

    return 0
}
