import std.process.Process
import std.text.String
import std.text.StringView
import std.testing.Test

test "process capture retains binary output and exit status" {
    var captured = Process::capture(String("printf 'a\\000b'; exit 7")).unwrap()

    Test::expect(captured.status == 7 && !captured.succeeded() && captured.output.length() == 3 &&
        captured.output.get(1).unwrap() == 0 as char &&
        Process::run(StringView::from_cstr("exit 7")).unwrap() == 7)
}
