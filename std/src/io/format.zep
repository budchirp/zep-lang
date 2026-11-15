import std.core.result.Result
import std.core.option.Option
import std.collections.vector.Vector
import std.text.string.String
import std.text.string.ConversionError
import std.text.view.StringView
import std.memory.Memory
import std.core.pair.Pair

public enum FormatError {
    MissingArgument { placeholder_index: i64 }
    ExtraArgument { argument_index: i64 }
    UnmatchedOpeningBrace { character_index: i64 }
    UnmatchedClosingBrace { character_index: i64 }
    Conversion { argument_index: i64, error: ConversionError }

    public:
        fn message() -> String {
            var detail = when (*self) {
                FormatError::MissingArgument { placeholder_index } ->
                    Pair("missing format argument at index ", placeholder_index),
                FormatError::ExtraArgument { argument_index } ->
                    Pair("unused format argument at index ", argument_index),
                FormatError::UnmatchedOpeningBrace { character_index } ->
                    Pair("unmatched opening brace at index ", character_index),
                FormatError::UnmatchedClosingBrace { character_index } ->
                    Pair("unmatched closing brace at index ", character_index),
                FormatError::Conversion { argument_index, error } ->
                    Pair("cannot format argument at index ", argument_index),
            }

            var mut message = String(detail.first)
            message.append(String::from(detail.second))

            return Memory::move(message)
        }
}

public struct FormatArguments {
    private:
        var values: Vector<Result<String, ConversionError>>

    public:
        fn FormatArguments() -> FormatArguments {
            return FormatArguments { values: Vector() }
        }

        fn add(value: cstr) mut -> void {
            values.push(Result::Ok { value: String(value) })
        }

        fn add(value: &String) mut -> void {
            values.push(Result::Ok { value: value })
        }

        fn add(value: &StringView) mut -> void {
            values.push(Result::Ok { value: String(value) })
        }

        fn add<T: i32>(value: T) mut -> void {
            values.push(Result::Ok { value: String::from(value) })
        }

        fn add<T: f64>(value: T) mut -> void {
            values.push(String::from(value as f64))
        }

        fn add(value: boolean) mut -> void {
            values.push(Result::Ok { value: String::from(value) })
        }

        fn add(value: char) mut -> void {
            values.push(Result::Ok { value: String::from(value) })
        }

        fn append_to(index: i64, output: &mut String) -> Result<void, FormatError> {
            return when (values.get(index)) {
                Option::None -> Result::Error { error: FormatError::MissingArgument { placeholder_index: index } },
                Option::Some { value } -> when (*value) {
                    Result::Error { error } -> Result::Error { error: FormatError::Conversion {
                        argument_index: index, error: error
                    } },
                    Result::Ok { value } -> {
                        output.append(value)

                        Result::Ok
                    },
                },
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

public fn format(pattern: &StringView, arguments: &FormatArguments) -> Result<String, FormatError> {
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
                        return Result::Error {
                            error: FormatError::UnmatchedOpeningBrace { character_index: character_index }
                        }
                    }

                    when (arguments.append_to(argument_index, output)) {
                        Result::Error { error } -> { return Result::Error { error: error } },
                        Result::Ok -> {},
                    }
                }

                argument_index = when (next == '{') {
                    true -> argument_index,
                    false -> argument_index + 1,
                }

                character_index = character_index + 2
            },
            (character == '}') -> {
                if (next != '}') {
                    return Result::Error {
                        error: FormatError::UnmatchedClosingBrace { character_index: character_index }
                    }
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
            Result::Error { error: FormatError::ExtraArgument { argument_index: argument_index } },
        else -> Result::Ok { value: Memory::move(output) },
    }
}

public fn format(pattern: cstr, arguments: &FormatArguments) -> Result<String, FormatError> {
    return format(StringView::from_cstr(pattern), arguments)
}

public fn format(pattern: &String, arguments: &FormatArguments) -> Result<String, FormatError> {
    return format(pattern.as_view(), arguments)
}
