import std.core.option.Option
import std.core.result.Result
import std.memory.Memory
import std.text.string.String

public enum FormatError {
    MissingArgument { placeholder_index: i32 }
    ExtraArgument { argument_index: i32 }
    UnmatchedOpeningBrace { character_index: i32 }
    UnmatchedClosingBrace { character_index: i32 }

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
    Text { value: cstr }
    SignedInteger { value: i64 }
    UnsignedInteger { value: u64 }
    FloatingPoint { value: f64 }
    Boolean { value: boolean }
    Character { value: char }

    public:
        fn render() -> String {
            return when (*self) {
                FormatArgument::Text { value } -> String(value),
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
        var ptr: *mut FormatArgument
        var length: i32
        var capacity: i32

        fn ensure_capacity(required_capacity: i32) mut -> void {
            if (required_capacity <= capacity) {
                return
            }

            var mut next_capacity: i32 = 4
            if (capacity != 0) {
                next_capacity = capacity * 2
            }
            while (next_capacity < required_capacity) {
                next_capacity = next_capacity * 2
            }

            var next_ptr = Memory::allocate_many<FormatArgument>(next_capacity as i64)
            if (ptr != null) {
                for (var mut index: i32 = 0; index < length; index = index + 1) {
                    next_ptr[index] = ptr[index]
                }

                Memory::free(ptr as *mut void)
            }

            ptr = next_ptr
            capacity = next_capacity
        }

        fn push(argument: FormatArgument) mut -> void {
            self->ensure_capacity(length + 1)
            ptr[length] = argument
            length = length + 1
        }

    public:
        fn FormatArguments() -> FormatArguments {
            return FormatArguments { ptr: null, length: 0, capacity: 0 }
        }

        fn add(value: cstr) mut -> void {
            self->push(FormatArgument::Text { value: value })
        }

        fn add(value: *String) mut -> void {
            self->add(value->as_cstr())
        }

        fn add(value: i8) mut -> void { self->add(value as i64) }
        fn add(value: i16) mut -> void { self->add(value as i64) }
        fn add(value: i32) mut -> void { self->add(value as i64) }

        fn add(value: i64) mut -> void {
            self->push(FormatArgument::SignedInteger { value: value })
        }

        fn add(value: u8) mut -> void { self->add(value as u64) }
        fn add(value: u16) mut -> void { self->add(value as u64) }
        fn add(value: u32) mut -> void { self->add(value as u64) }

        fn add(value: u64) mut -> void {
            self->push(FormatArgument::UnsignedInteger { value: value })
        }

        fn add(value: f32) mut -> void { self->add(value as f64) }

        fn add(value: f64) mut -> void {
            self->push(FormatArgument::FloatingPoint { value: value })
        }

        fn add(value: boolean) mut -> void {
            self->push(FormatArgument::Boolean { value: value })
        }

        fn add(value: char) mut -> void {
            self->push(FormatArgument::Character { value: value })
        }

        fn render(index: i32) -> Option<String> {
            if (index < 0 || index >= length) {
                return Option<String>::None
            }

            return Option<String>::Some { value: ptr[index].render() }
        }

        fn size() -> i32 {
            return length
        }

        fn is_empty() -> boolean {
            return length == 0
        }

        fn clear() mut -> void {
            length = 0
        }

        fn ~FormatArguments() -> void {
            if (ptr != null) {
                Memory::free(ptr as *mut void)
            }
        }
}

public fn sprint(format: cstr, arguments: *FormatArguments) -> Result<String, FormatError> {
    var mut output = String()
    var mut character_index: i32 = 0
    var mut argument_index: i32 = 0

    while (format[character_index] != (0 as char)) {
        var character = format[character_index]

        if (character == '{') {
            if (format[character_index + 1] == '{') {
                output.push('{')
                character_index = character_index + 2
            } else {
                if (format[character_index + 1] != '}') {
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
            if (format[character_index + 1] != '}') {
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

    if (argument_index < arguments->size()) {
        return Result<String, FormatError>::Error {
            error: FormatError::ExtraArgument { argument_index: argument_index }
        }
    }

    return Result<String, FormatError>::Ok { value: output }
}

public fn sprint(format: cstr) -> Result<String, FormatError> {
    var arguments = FormatArguments()
    return sprint(format, &arguments)
}
