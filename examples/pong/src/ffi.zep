public struct Color {
    public:
        var r: u8
        var g: u8
        var b: u8
        var a: u8
}

public struct Vector2 {
    public:
        var x: f32
        var y: f32
}

public extern fn InitWindow(width: i32, height: i32, title: cstr) -> void
public extern fn CloseWindow() -> void
public extern fn WindowShouldClose() -> boolean
public extern fn BeginDrawing() -> void
public extern fn EndDrawing() -> void
public extern fn ClearBackground(color: Color) -> void
public extern fn DrawCircle(center_x: i32, center_y: i32, radius: f32, color: Color) -> void
public extern fn DrawCircleV(center: Vector2, radius: f32, color: Color) -> void
public extern fn DrawRectangle(x: i32, y: i32, width: i32, height: i32, color: Color) -> void
public extern fn DrawLine(start_x: i32, start_y: i32, end_x: i32, end_y: i32, color: Color) -> void
public extern fn DrawText(text: cstr, x: i32, y: i32, font_size: i32, color: Color) -> void
public extern fn DrawFPS(x: i32, y: i32) -> void
public extern fn SetTargetFPS(fps: i32) -> void
public extern fn IsKeyDown(key: i32) -> boolean
public extern fn IsKeyPressed(key: i32) -> boolean
public extern fn GetFrameTime() -> f32
public extern fn TextFormat(text: cstr, ...) -> cstr
