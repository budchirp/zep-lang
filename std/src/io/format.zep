import std.core.error.Error
import std.core.option.Option
import std.collections.vector.Vector
import std.text.string.String
import std.text.string.ConversionError
import std.text.view.StringView
import std.memory.Memory

public enum FormatFailure : i32 {
    MissingArgument = 0
    ExtraArgument = 1
    UnmatchedOpeningBrace = 2
    UnmatchedClosingBrace = 3
}

public struct FormatError : Error {
    public:
        var reason: FormatFailure
        var index: i64

        fn message() -> String {
            var mut message = String(when (reason) {
                FormatFailure::MissingArgument -> "missing format argument at index ",
                FormatFailure::ExtraArgument -> "unused format argument at index ",
                FormatFailure::UnmatchedOpeningBrace -> "unmatched opening brace at index ",
                FormatFailure::UnmatchedClosingBrace -> "unmatched closing brace at index ",
            })
            message.append(String::from(index))

            return Memory::move(message)
        }
}

public struct FormatArguments {
    private:
        var values: Vector<String>

    public:
        fn FormatArguments() -> FormatArguments {
            return FormatArguments { values: Vector() }
        }

        fn add(value: cstr) mut -> void {
            values.push(String(value))
        }

        fn add(value: &String) mut -> void {
            values.push(value)
        }

        fn add(value: &StringView) mut -> void {
            values.push(String(value))
        }

        fn add<T: i32>(value: T) mut -> void {
            values.push(String::from(value))
        }

        @throws(ConversionError)
        fn add<T: f64>(value: T) mut -> void {
            values.push(try String::from(value as f64))
        }

        fn add(value: boolean) mut -> void {
            values.push(String::from(value))
        }

        fn add(value: char) mut -> void {
            values.push(String::from(value))
        }

        @throws(FormatError)
        fn append_to(index: i64, output: &mut String) -> void {
            when (values.get(index)) {
                Option::None -> throw FormatError { reason: FormatFailure::MissingArgument, index: index },
                Option::Some { value } -> output.append(*value),
            }
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
}

@throws(FormatError)
public fn format(pattern: &StringView, arguments: &FormatArguments) -> String {
    var mut output = String()
    var mut character_index: i64 = 0
    var mut argument_index: i64 = 0

    while (character_index < pattern.length()) {
        var character = pattern.get(character_index).unwrap()
        var next = pattern.get(character_index + 1).unwrap_or(0 as char)

        when {
            (character == '{') -> {
                if (next == '{') {
                    output.push('{')
                } else {
                    if (next != '}') {
                        throw FormatError { reason: FormatFailure::UnmatchedOpeningBrace, index: character_index }
                    }

                    try arguments.append_to(argument_index, output)
                }

                argument_index = when (next == '{') {
                    true -> argument_index,
                    false -> argument_index + 1,
                }

                character_index = character_index + 2
            },
            (character == '}') -> {
                if (next != '}') {
                    throw FormatError { reason: FormatFailure::UnmatchedClosingBrace, index: character_index }
                }

                output.push('}')
                character_index = character_index + 2
            },
            else -> {
                output.push(character)
                character_index = character_index + 1
            },
        }
    }

    return when {
        (argument_index < arguments.length()) ->
            throw FormatError { reason: FormatFailure::ExtraArgument, index: argument_index },
        else -> Memory::move(output),
    }
}

@throws(FormatError)
public fn format(pattern: cstr, arguments: &FormatArguments) -> String {
    return try format(StringView::from_cstr(pattern), arguments)
}

@throws(FormatError)
public fn format(pattern: &String, arguments: &FormatArguments) -> String {
    return try format(pattern.as_view(), arguments)
}
