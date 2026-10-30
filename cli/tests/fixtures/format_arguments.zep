import std.io.format.FormatArguments
import std.io.format.sprint
import std.text.string.String

public fn main() -> i32 {
    var mut arguments = FormatArguments()
    if (!arguments.is_empty() || arguments.size() != 0 ||
        arguments.render(-1).is_some() || arguments.render(0).is_some()) {
        return 1
    }

    for (var mut index: i32 = 0; index < 40; index = index + 1) {
        arguments.add(index)
    }

    if (arguments.is_empty() || arguments.size() != 40 || arguments.render(40).is_some()) {
        return 2
    }

    for (var mut index: i32 = 0; index < 40; index = index + 1) {
        var rendered = arguments.render(index)
        if (rendered.is_none()) {
            return 3
        }

        var actual = rendered.unwrap()
        var expected = String::from(index)
        if (!actual.equals(&expected)) {
            return 3
        }
    }

    arguments.clear()
    if (!arguments.is_empty() || arguments.size() != 0 || arguments.render(0).is_some()) {
        return 4
    }

    var text = String("owned")
    arguments.add("borrowed")
    arguments.add(&text)
    arguments.add(-1 as i8)
    arguments.add(-2 as i16)
    arguments.add(-3 as i32)
    arguments.add(-4 as i64)
    arguments.add(1 as u8)
    arguments.add(2 as u16)
    arguments.add(3 as u32)
    arguments.add(4 as u64)
    arguments.add(1.5 as f32)
    arguments.add(2.5 as f64)
    arguments.add(true)
    arguments.add('Z')

    var formatted = sprint("{} {} {} {} {} {} {} {} {} {} {} {} {} {}", &arguments)
    if (formatted.is_error()) {
        return 5
    }

    var formatted_text = formatted.unwrap()
    if (!formatted_text.equals("borrowed owned -1 -2 -3 -4 1 2 3 4 1.5 2.5 true Z")) {
        return 5
    }

    for (var mut iteration: i32 = 0; iteration < 20; iteration = iteration + 1) {
        arguments.clear()
        arguments.add(iteration)
        var rendered = arguments.render(0)
        if (arguments.size() != 1 || rendered.is_none()) {
            return 6
        }

        var actual = rendered.unwrap()
        var expected = String::from(iteration)
        if (!actual.equals(&expected)) {
            return 6
        }
    }

    return 0
}
