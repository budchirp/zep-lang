import graphics.Window
import pong.PongGame

public fn main() -> i32 {
    var window = Window(900, 520, "Zep Pong")
    var mut game = PongGame(window.width, window.height)

    while (!window.should_close()) {
        game.update()
        game.draw()
    }

    return 0
}
