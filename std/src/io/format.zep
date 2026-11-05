import std.core.option.Option
import std.core.result.Result
import std.collections.vector.Vector
import std.text.string.String

public enum FormatError {
    MissingArgument { placeholder_index: i64 }
    ExtraArgument { argument_index: i64 }
    UnmatchedOpeningBrace { character_index: i64 }
    UnmatchedClosingBrace { character_index: i64 }

    public:
        fn message() -> String {
            var mut message = when (*self) {
                FormatError::MissingArgument { placeholder_index } ->
                    String("missing format argument at index "),
                FormatError::ExtraArgument { argument_index } ->
                    String("unused format argument at index "),
                FormatError::UnmatchedOpeningBrace { character_index } ->
                    String("unmatched opening brace at index "),
                FormatError::UnmatchedClosingBrace { character_index } ->
                    String("unmatched closing brace at index "),
            }

            var index = when (*self) {
                FormatError::MissingArgument { placeholder_index } -> placeholder_index,
                FormatError::ExtraArgument { argument_index } -> argument_index,
                FormatError::UnmatchedOpeningBrace { character_index } -> character_index,
                FormatError::UnmatchedClosingBrace { character_index } -> character_index,
            }

            message.append(String::from(index).as_cstr())
            return message
        }
}

enum FormatArgument {
    Text { value: String }
    SignedInteger { value: i64 }
    UnsignedInteger { value: u64 }
    FloatingPoint { value: f64 }
    Boolean { value: boolean }
    Character { value: char }

    public:
        fn render() -> String {
            return when (*self) {
                FormatArgument::Text { value } -> value,
                FormatArgument::SignedInteger { value } -> String::from(value),
                FormatArgument::UnsignedInteger { value } -> String::from(value),
                FormatArgument::FloatingPoint { value } -> String::from(value),
                FormatArgument::Boolean { value } -> String::from(value),
                FormatArgument::Character { value } -> String::from(value),
            }
        }
}

public struct FormatArguments {
    private:
        var values: Vector<FormatArgument>

    public:
        fn FormatArguments() -> FormatArguments {
            return FormatArguments { values: Vector<FormatArgument>() }
        }

        fn add(value: cstr) mut -> void {
            values.push(FormatArgument::Text { value: String(value) })
        }

        fn add(value: *String) mut -> void {
            var view = value->as_view()
            values.push(FormatArgument::Text { value: String(&view) })
        }

        fn add(value: i8) mut -> void { self->add(value as i64) }
        fn add(value: i16) mut -> void { self->add(value as i64) }
        fn add(value: i32) mut -> void { self->add(value as i64) }

        fn add(value: i64) mut -> void {
            values.push(FormatArgument::SignedInteger { value: value })
        }

        fn add(value: u8) mut -> void { self->add(value as u64) }
        fn add(value: u16) mut -> void { self->add(value as u64) }
        fn add(value: u32) mut -> void { self->add(value as u64) }

        fn add(value: u64) mut -> void {
            values.push(FormatArgument::UnsignedInteger { value: value })
        }

        fn add(value: f32) mut -> void { self->add(value as f64) }

        fn add(value: f64) mut -> void {
            values.push(FormatArgument::FloatingPoint { value: value })
        }

        fn add(value: boolean) mut -> void {
            values.push(FormatArgument::Boolean { value: value })
        }

        fn add(value: char) mut -> void {
            values.push(FormatArgument::Character { value: value })
        }

        fn render(index: i64) -> Option<String> {
            var argument = values.get(index)
            if (argument.is_none()) {
                return Option<String>::None
            }

            return Option<String>::Some { value: argument.unwrap()->render() }
        }

        fn length() -> i64 {
            return values.length()
        }

        fn is_empty() -> boolean {
            return values.is_empty()
        }

        fn clear() mut -> void {
            values.clear()
        }

        fn ~FormatArguments() -> void {}
}

public fn format(pattern: cstr, arguments: *FormatArguments) -> Result<String, FormatError> {
    var mut output = String()
    var mut character_index: i64 = 0
    var mut argument_index: i64 = 0

    while (pattern[character_index] != (0 as char)) {
        var character = pattern[character_index]

        if (character == '{') {
            if (pattern[character_index + 1] == '{') {
                output.push('{')
                character_index = character_index + 2
            } else {
                if (pattern[character_index + 1] != '}') {
                    return Result<String, FormatError>::Error {
                        error: FormatError::UnmatchedOpeningBrace {
                            character_index: character_index
                        }
                    }
                }

                var argument = arguments->render(argument_index)
                if (argument.is_none()) {
                    return Result<String, FormatError>::Error {
                        error: FormatError::MissingArgument {
                            placeholder_index: argument_index
                        }
                    }
                }

                var rendered = argument.unwrap()
                output.append(&rendered)
                argument_index = argument_index + 1
                character_index = character_index + 2
            }
        } else if (character == '}') {
            if (pattern[character_index + 1] != '}') {
                return Result<String, FormatError>::Error {
                    error: FormatError::UnmatchedClosingBrace {
                        character_index: character_index
                    }
                }
            }

            output.push('}')
            character_index = character_index + 2
        } else {
            output.push(character)
            character_index = character_index + 1
        }
    }

    if (argument_index < arguments->length()) {
        return Result<String, FormatError>::Error {
            error: FormatError::ExtraArgument { argument_index: argument_index }
        }
    }

    return Result<String, FormatError>::Ok { value: output }
}

public fn format(pattern: cstr) -> Result<String, FormatError> {
    var arguments = FormatArguments()
    return format(pattern, &arguments)
}
