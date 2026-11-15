import ffi.Color
import ffi.GetFrameTime
import graphics.Frame
import graphics.Rect
import graphics.Key
import graphics.Palette
import std.math.Math

public struct Paddle {
    public:
        var rect: Rect
        var up_key: Key
        var down_key: Key
        var color: Color

        fn Paddle(x: f32, y: f32, up_key: Key, down_key: Key, color: Color) -> Paddle {
            return Paddle { rect: Rect(x, y, 18.0, 92.0), up_key: up_key, down_key: down_key, color: color }
        }

        fn update(delta: f32, height: f32) mut -> void {
            var distance = delta * 360.0

            if (up_key.down()) {
                rect.y = rect.y - distance
            }

            if (down_key.down()) {
                rect.y = rect.y + distance
            }

            rect.y = Math::clamp(rect.y, 0.0, height - rect.height)
        }

        fn draw(frame: &Frame) -> void {
            frame.rectangle(rect, color)
        }
}

public struct Ball {
    public:
        var rect: Rect
        var velocity_x: f32
        var velocity_y: f32

        fn Ball(x: f32, y: f32) -> Ball {
            return Ball { rect: Rect(x, y, 24.0, 24.0), velocity_x: 260.0, velocity_y: 140.0 }
        }

        fn reset(width: f32, height: f32, direction: f32, vertical_speed: f32) mut -> void {
            rect.x = (width - rect.width) / 2.0
            rect.y = (height - rect.height) / 2.0
            velocity_x = 260.0 * direction
            velocity_y = vertical_speed
        }

        fn update(delta: f32, height: f32) mut -> void {
            rect.x = rect.x + velocity_x * delta
            rect.y = rect.y + velocity_y * delta

            if (rect.y < 0.0 || rect.bottom() > height) {
                rect.y = Math::clamp(rect.y, 0.0, height - rect.height)
                velocity_y = -velocity_y
            }
        }

        fn bounce(paddle: &Paddle, direction: f32) mut -> void {
            if (rect.overlaps(paddle.rect)) {
                rect.x = when (direction > 0.0) {
                    true -> paddle.rect.right(),
                    false -> paddle.rect.x - rect.width,
                }

                velocity_x = 310.0 * direction
                velocity_y = Math::clamp(
                    (rect.center_y() - paddle.rect.center_y()) / (paddle.rect.height / 2.0),
                    -1.0, 1.0) * 320.0
            }
        }

        fn draw(frame: &Frame) -> void {
            frame.rectangle(rect, Palette::Ball.value)
        }
}

public struct ScoreBoard {
    public:
        var left: i32
        var right: i32

        fn ScoreBoard() -> ScoreBoard {
            return ScoreBoard { left: 0, right: 0 }
        }

        fn draw(frame: &Frame) -> void {
            frame.number(left, 360, 28, 48, Palette::Text.value)
            frame.text(":", 438, 30, 44, Palette::Muted.value)
            frame.number(right, 500, 28, 48, Palette::Text.value)
        }
}

public struct PongGame {
    private:
        var width: f32
        var height: f32
        var left_paddle: Paddle
        var right_paddle: Paddle
        var ball: Ball
        var score: ScoreBoard

        fn serve_y() -> f32 {
            return when ((score.left + score.right) % 4) {
                0 -> 140.0,
                1 -> -180.0,
                2 -> 210.0,
                else -> -120.0,
            }
        }

    public:
        fn PongGame(width: f32, height: f32) -> PongGame {
            return PongGame {
                width: width,
                height: height,
                left_paddle: Paddle(40.0, (height - 92.0) / 2.0, Key::W, Key::S, Palette::Left.value),
                right_paddle: Paddle(width - 58.0, (height - 92.0) / 2.0, Key::Up, Key::Down, Palette::Right.value),
                ball: Ball((width - 24.0) / 2.0, (height - 24.0) / 2.0),
                score: ScoreBoard()
            }
        }

        fn update() mut -> void {
            var delta = GetFrameTime()

            if (Key::R.pressed()) {
                score = ScoreBoard()
                ball.reset(width, height, 1.0, 140.0)
            }

            left_paddle.update(delta, height)
            right_paddle.update(delta, height)
            ball.update(delta, height)
            ball.bounce(left_paddle, 1.0)
            ball.bounce(right_paddle, -1.0)

            if (ball.rect.right() < 0.0) {
                score.right = score.right + 1
                ball.reset(width, height, 1.0, serve_y())
            }

            if (ball.rect.x > width) {
                score.left = score.left + 1
                ball.reset(width, height, -1.0, serve_y())
            }
        }

        fn draw() -> void {
            var frame = Frame()
            var width = self->width as i32
            var height = self->height as i32

            frame.clear(Palette::Background.value)

            for (var mut y = 12; y < height; y = y + 28) {
                frame.line(width / 2, y, width / 2, y + 14, Palette::Panel.value)
            }

            left_paddle.draw(frame)
            right_paddle.draw(frame)
            ball.draw(frame)
            score.draw(frame)

            frame.text("left: W/S", 24, height - 34, 20, Palette::Muted.value)
            frame.text("right: arrows", width - 164, height - 34, 20, Palette::Muted.value)
            frame.text("R reset", width / 2 - 40, height - 34, 20, Palette::Muted.value)
            frame.fps(12, 12)
        }
}
