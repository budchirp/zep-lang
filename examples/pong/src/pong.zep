import ffi.Color
import graphics.Renderer
import graphics.Rect
import graphics.Key
import graphics.ball_color
import graphics.background_color
import graphics.panel_color
import graphics.left_color
import graphics.right_color
import graphics.text_color
import graphics.muted_color
import graphics.key_is_down
import graphics.key_is_pressed
import graphics.frame_time

public interface Drawable {
    public:
        fn draw(renderer: *mut Renderer) -> void
}

public struct Entity {
    public:
        var rect: Rect

        fn Entity(x: f32, y: f32, width: f32, height: f32) -> Entity {
            return Entity { rect: Rect(x, y, width, height) }
        }

        fn center_x() -> f32 {
            return rect.x + rect.width / 2.0 as f32
        }

        fn center_y() -> f32 {
            return rect.y + rect.height / 2.0 as f32
        }
}

public struct Paddle : Entity, Drawable {
    public:
        var speed: f32
        var up_key: Key
        var down_key: Key
        var color: Color

        fn Paddle(x: f32, y: f32, up_key: Key, down_key: Key, color: Color) -> Paddle {
            return Paddle {
                rect: Rect(x, y, 18.0 as f32, 92.0 as f32),
                speed: 360.0 as f32,
                up_key: up_key,
                down_key: down_key,
                color: color
            }
        }

        fn update(delta: f32, field_height: f32) mut -> void {
            var up = up_key
            if (key_is_down(up)) {
                rect.y = rect.y - speed * delta
            }

            var down = down_key
            if (key_is_down(down)) {
                rect.y = rect.y + speed * delta
            }

            if (rect.y < 0.0 as f32) {
                rect.y = 0.0 as f32
            }

            if (rect.y + rect.height > field_height) {
                rect.y = field_height - rect.height
            }
        }

        override fn draw(renderer: *mut Renderer) -> void {
            var r = rect
            var c = color
            renderer->rectangle(r, c)
        }
}

public struct Ball : Entity, Drawable {
    public:
        var velocity_x: f32
        var velocity_y: f32
        var radius: f32
        var color: Color

        fn Ball(x: f32, y: f32) -> Ball {
            return Ball {
                rect: Rect(x, y, 24.0 as f32, 24.0 as f32),
                velocity_x: 260.0 as f32,
                velocity_y: 170.0 as f32,
                radius: 12.0 as f32,
                color: ball_color()
            }
        }

        fn reset(field_width: f32, field_height: f32, direction: f32, vertical_speed: f32) mut -> void {
            rect.x = field_width / 2.0 as f32 - radius
            rect.y = field_height / 2.0 as f32 - radius
            velocity_x = 260.0 as f32 * direction
            velocity_y = vertical_speed
        }

        fn update(delta: f32) mut -> void {
            rect.x = rect.x + velocity_x * delta
            rect.y = rect.y + velocity_y * delta
        }

        fn bounce_y() mut -> void {
            velocity_y = 0.0 as f32 - velocity_y
        }

        fn bounce_x() mut -> void {
            velocity_x = 0.0 as f32 - velocity_x
        }

        override fn draw(renderer: *mut Renderer) -> void {
            var r = rect
            var c = color
            renderer->rectangle(r, c)
        }
}

public struct ScoreBoard : Drawable {
    public:
        var left: i32
        var right: i32

        fn ScoreBoard() -> ScoreBoard {
            return ScoreBoard { left: 0, right: 0 }
        }

        fn reset() mut -> void {
            left = 0
            right = 0
        }

        override fn draw(renderer: *mut Renderer) -> void {
            renderer->number(left, 360, 28, 48, text_color())
            renderer->text(":", 438, 30, 44, muted_color())
            renderer->number(right, 500, 28, 48, text_color())
        }
}

public struct PongGame {
    public:
        var width: f32
        var height: f32

    private:
        var renderer: Renderer
        var left_paddle: Paddle
        var right_paddle: Paddle
        var ball: Ball
        var score: ScoreBoard

    public:
        fn PongGame(width: i32, height: i32) -> PongGame {
            var mut game = PongGame {
                width: width as f32,
                height: height as f32,
                renderer: Renderer(),
                left_paddle: Paddle(40.0 as f32, 214.0 as f32,
                                   Key::W, Key::S, left_color()),
                right_paddle: Paddle(842.0 as f32, 214.0 as f32,
                                    Key::Up, Key::Down, right_color()),
                ball: Ball(0.0 as f32, 0.0 as f32),
                score: ScoreBoard()
            }

            game.ball.reset(game.width, game.height, 1.0 as f32, 140.0 as f32)

            return game
        }

        fn update() mut -> void {
            var delta = frame_time()

            if (key_is_pressed(Key::R)) {
                score.reset()
                ball.reset(width, height, 1.0 as f32, 140.0 as f32)
            }

            left_paddle.update(delta, height)
            right_paddle.update(delta, height)
            ball.update(delta)

            if (ball.rect.y < 0.0 as f32) {
                ball.rect.y = 0.0 as f32
                ball.bounce_y()
            }

            if (ball.rect.y + ball.rect.height > height) {
                ball.rect.y = height - ball.rect.height
                ball.bounce_y()
            }

            if (overlaps(&left_paddle)) {
                ball.rect.x = left_paddle.rect.x + left_paddle.rect.width
                bounce_from_paddle(&left_paddle, 1.0 as f32)
            }

            if (overlaps(&right_paddle)) {
                ball.rect.x = right_paddle.rect.x - ball.rect.width
                bounce_from_paddle(&right_paddle, -1.0 as f32)
            }

            if (ball.rect.x + ball.rect.width < 0.0 as f32) {
                score.right = score.right + 1
                ball.reset(width, height, 1.0 as f32, serve_y())
            }

            if (ball.rect.x > width) {
                score.left = score.left + 1
                ball.reset(width, height, -1.0 as f32, serve_y())
            }
        }

        fn draw() mut -> void {
            renderer.begin()
            renderer.clear(background_color())

            renderer.rectangle(Rect(0.0 as f32, 0.0 as f32, width, height), background_color())

            for (var mut y: i32 = 12; y < height as i32; y = y + 28) {
                renderer.line(width as i32 / 2, y, width as i32 / 2, y + 14, panel_color())
            }

            renderer.rectangle(left_paddle.rect, left_color())
            renderer.rectangle(right_paddle.rect, right_color())
            renderer.rectangle(ball.rect, ball_color())
            score.draw(&mut renderer)

            renderer.text("left: W/S", 24, height as i32 - 34, 20, muted_color())
            renderer.text("right: arrows", width as i32 - 164, height as i32 - 34, 20, muted_color())
            renderer.text("R reset", width as i32 / 2 - 40, height as i32 - 34, 20, muted_color())
            renderer.fps(12, 12)

            renderer.finish()
        }

    private:
        fn overlaps(paddle: *Paddle) -> boolean {
            return ball.rect.x < paddle->rect.x + paddle->rect.width &&
                   ball.rect.x + ball.rect.width > paddle->rect.x &&
                   ball.rect.y < paddle->rect.y + paddle->rect.height &&
                   ball.rect.y + ball.rect.height > paddle->rect.y
        }

        fn bounce_from_paddle(paddle: *Paddle, direction: f32) mut -> void {
            var ball_center = ball.rect.y + ball.radius
            var paddle_center = paddle->rect.y + paddle->rect.height / (2.0 as f32)
            var mut offset = (ball_center - paddle_center) / (paddle->rect.height / (2.0 as f32))

            if (offset < -1.0 as f32) {
                offset = -1.0 as f32
            }

            if (offset > 1.0 as f32) {
                offset = 1.0 as f32
            }

            ball.velocity_x = 310.0 as f32 * direction
            ball.velocity_y = offset * 320.0 as f32
        }

        fn serve_y() -> f32 {
            var total = score.left + score.right
            var choice = total % 4

            if (choice == 0) {
                return 140.0 as f32
            }

            if (choice == 1) {
                return -180.0 as f32
            }

            if (choice == 2) {
                return 210.0 as f32
            }

            return -120.0 as f32
        }
}
