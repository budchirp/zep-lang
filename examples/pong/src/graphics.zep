import ffi.Color
import ffi.Vector2
import ffi.InitWindow
import ffi.CloseWindow
import ffi.WindowShouldClose
import ffi.BeginDrawing
import ffi.EndDrawing
import ffi.ClearBackground
import ffi.DrawCircle
import ffi.DrawCircleV
import ffi.DrawRectangle
import ffi.DrawLine
import ffi.DrawText
import ffi.DrawFPS
import ffi.SetTargetFPS
import ffi.IsKeyDown
import ffi.IsKeyPressed
import ffi.GetFrameTime
import ffi.TextFormat

public enum Key : i32 {
    W = 87
    S = 83
    R = 82
    Up = 265
    Down = 264
}

fn rgba(r: u8, g: u8, b: u8, a: u8) -> Color {
    return Color { r: r, g: g, b: b, a: a }
}

public fn background_color() -> Color {
    return rgba(18, 20, 24, 255)
}

public fn panel_color() -> Color {
    return rgba(35, 38, 46, 255)
}

public fn left_color() -> Color {
    return rgba(92, 214, 160, 255)
}

public fn right_color() -> Color {
    return rgba(246, 190, 92, 255)
}

public fn ball_color() -> Color {
    return rgba(235, 240, 245, 255)
}

public fn text_color() -> Color {
    return rgba(220, 226, 235, 255)
}

public fn muted_color() -> Color {
    return rgba(96, 104, 120, 255)
}

public fn key_is_down(key: Key) -> boolean {
    return IsKeyDown(key.value)
}

public fn key_is_pressed(key: Key) -> boolean {
    return IsKeyPressed(key.value)
}

public fn frame_time() -> f32 {
    return GetFrameTime()
}

public struct Rect {
    public:
        var x: f32
        var y: f32
        var width: f32
        var height: f32

        fn Rect(x: f32, y: f32, width: f32, height: f32) -> Rect {
            return Rect { x: x, y: y, width: width, height: height }
        }

        fn left() -> f32 {
            return x
        }

        fn right() -> f32 {
            return x + width
        }

        fn top() -> f32 {
            return y
        }

        fn bottom() -> f32 {
            return y + height
        }
}

public struct Window {
    public:
        var width: i32
        var height: i32

        fn Window(width: i32, height: i32, title: cstr) -> Window {
            InitWindow(width, height, title)
            SetTargetFPS(2147483647)

            return Window { width: width, height: height }
        }

        fn should_close() -> boolean {
            return WindowShouldClose()
        }

        fn ~Window() -> void {
            CloseWindow()
        }
}

public struct Renderer {
    public:
        fn Renderer() -> Renderer {
            return Renderer {}
        }

        fn begin() mut -> void {
            BeginDrawing()
        }

        fn finish() mut -> void {
            EndDrawing()
        }

        fn clear(color: Color) mut -> void {
            ClearBackground(color)
        }

        fn rectangle(rect: Rect, color: Color) mut -> void {
            DrawRectangle(rect.x as i32, rect.y as i32,
                         rect.width as i32, rect.height as i32,
                         color)
        }

        fn circle(x: f32, y: f32, radius: f32, color: Color) mut -> void {
            DrawCircle(x as i32, y as i32, radius, color)
        }

        fn line(start_x: i32, start_y: i32, end_x: i32, end_y: i32, color: Color) mut -> void {
            DrawLine(start_x, start_y, end_x, end_y, color)
        }

        fn text(text: cstr, x: i32, y: i32, size: i32, color: Color) mut -> void {
            DrawText(text, x, y, size, color)
        }

        fn number(value: i32, x: i32, y: i32, size: i32, color: Color) mut -> void {
            DrawText(TextFormat("%d", value), x, y, size, color)
        }

        fn fps(x: i32, y: i32) mut -> void {
            DrawFPS(x, y)
        }
}
