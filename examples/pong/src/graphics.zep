import ffi.Color
import ffi.InitWindow
import ffi.CloseWindow
import ffi.WindowShouldClose
import ffi.BeginDrawing
import ffi.EndDrawing
import ffi.ClearBackground
import ffi.DrawRectangle
import ffi.DrawLine
import ffi.DrawText
import ffi.DrawFPS
import ffi.SetTargetFPS
import ffi.IsKeyDown
import ffi.IsKeyPressed
import ffi.TextFormat

public enum Key : i32 {
    W = 87
    S = 83
    R = 82
    Up = 265
    Down = 264

    public:
        fn down() -> boolean {
            return IsKeyDown((*self).value)
        }

        fn pressed() -> boolean {
            return IsKeyPressed((*self).value)
        }
}

public enum Palette : Color {
    Background = Color { r: 18, g: 20, b: 24, a: 255 }
    Panel = Color { r: 35, g: 38, b: 46, a: 255 }
    Left = Color { r: 92, g: 214, b: 160, a: 255 }
    Right = Color { r: 246, g: 190, b: 92, a: 255 }
    Ball = Color { r: 235, g: 240, b: 245, a: 255 }
    Text = Color { r: 220, g: 226, b: 235, a: 255 }
    Muted = Color { r: 96, g: 104, b: 120, a: 255 }
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

        fn center_y() -> f32 {
            return y + height / 2.0
        }

        fn right() -> f32 {
            return x + width
        }

        fn bottom() -> f32 {
            return y + height
        }

        fn overlaps(other: &Rect) -> boolean {
            return x < other.right() && self->right() > other.x &&
                   y < other.bottom() && self->bottom() > other.y
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

        fn Window(source: &Window) -> Window = delete

        fn should_close() -> boolean {
            return WindowShouldClose()
        }

        fn ~Window() -> void {
            CloseWindow()
        }
}

public struct Frame {
    public:
        fn Frame() -> Frame {
            BeginDrawing()

            return Frame {}
        }

        fn Frame(source: &Frame) -> Frame = delete

        fn clear(color: Color) -> void {
            ClearBackground(color)
        }

        fn rectangle(rect: &Rect, color: Color) -> void {
            DrawRectangle(rect.x as i32, rect.y as i32, rect.width as i32, rect.height as i32, color)
        }

        fn line(x: i32, y: i32, end_x: i32, end_y: i32, color: Color) -> void {
            DrawLine(x, y, end_x, end_y, color)
        }

        fn text(text: cstr, x: i32, y: i32, size: i32, color: Color) -> void {
            DrawText(text, x, y, size, color)
        }

        fn number(value: i32, x: i32, y: i32, size: i32, color: Color) -> void {
            self->text(TextFormat("%d", value), x, y, size, color)
        }

        fn fps(x: i32, y: i32) -> void {
            DrawFPS(x, y)
        }

        fn ~Frame() -> void {
            EndDrawing()
        }
}
