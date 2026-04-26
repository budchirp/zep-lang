struct Point {
    x: i32
    y: i32
}

fn main(): i32 {
    var p = Point { x: 10, y: 20 };
    return p.x + p.y;
}
