# Zep

Systems programming language with manual memory management, generics, interfaces, and zero-cost abstractions. Compiles to native code via LLVM.

## Features

- **Deterministic cleanup** — destructors (`fn ~Type() -> void`) and `defer` statements run automatically in reverse order on scope exit
- **Generics** — `fn identity<T>(value: T) -> T`, `struct Container<T>`
- **Interfaces** — structural contracts with dynamic dispatch
- **Enums with payloads** — `Option<T>::Some { value: T }`, `Result<T, E>::Ok { value: T }`, or backed variants (`enum Key : i32`)
- **Struct inheritance** — single inheritance with `override fn`
- **defer** — scope-exit cleanup in reverse order
- **when** — multi-branch matching with guards and destructuring
- **Block expressions** — `do { ... }` blocks that evaluate to expressions
- **Primitive facades** — `32.to_string()`, `"zep".to_string()`, `true.to_string()`
- **C FFI** — `extern fn`, `extern var` with variadic support
- **Compile-time built-ins** — `#sizeof(T)`, `#length(array)`, `#asm(...)`
- **Language Server Protocol** — integrated `zep lsp` with completion, hover, diagnostics, and semantic tokens

## Example

```zep
import std.ffi.printf

struct Point {
    public:
        var x: i32
        var y: i32

        fn Point(x: i32, y: i32) -> Point {
            return Point { x: x, y: y }
        }

        fn distance_squared() -> i32 {
            return x * x + y * y
        }
}

interface Shape {
    public:
        fn area() -> f64
}

struct Circle : Shape {
    public:
        var radius: f64

        fn Circle(radius: f64) -> Circle {
            return Circle { radius: radius }
        }

        override fn area() -> f64 {
            return 3.14159 * radius * radius
        }
}

public fn main() -> i32 {
    var point = Point(3, 4)
    printf("distance squared: %d\n", point.distance_squared())

    var circle = Circle(5.0)
    printf("area: %f\n", circle.area())

    return 0
}
```

## Build

Prerequisites: CMake 3.30+, Ninja, Clang with C++26 support

```bash
cmake --preset debug
cmake --build cmake-build-debug
```

## Test

```bash
ctest --test-dir cmake-build-debug --output-on-failure
```

Use `ctest --test-dir cmake-build-debug -N` to list the currently discovered tests.

## Optimize

Use `-o` with levels `0`, `1`, `2`, or `3`:

```bash
cmake-build-debug/cli/zep build -o 2
cmake-build-debug/cli/zep compile --input path/to/file.zep -o 3
```

Level `0` is the default. Higher levels run LLVM optimization passes before object emission.

## Run

Run the Pong example project:

```bash
cd examples/pong
../../cmake-build-debug/cli/zep build
./build/pong
```

Compile a single source file directly:

```bash
cmake-build-debug/cli/zep compile --input examples/pong/src/main.zep --output build/main.o
```

Launch the Language Server:

```bash
cmake-build-debug/cli/zep lsp
```

## CLI Commands

- `zep build [-o 0|1|2|3] [-v] [--project <path>]`: Build a project defined by `zep.json`
- `zep compile --input <file> [--output <file>] [-o 0|1|2|3] [-v]`: Compile a source file to an object file
- `zep lsp`: Start the Language Server Protocol server (stdin/stdout)
- `zep fetch`: Download and resolve package dependencies
- `zep install`: Install the compiler and standard library to `~/.local/share/zep/`

Projects use `zep.json`:

```json
{
    "name": "project",
    "version": "0.1.0",
    "type": "executable",
    "libs": {
        "std": "0.0.1"
    },
    "target": [
        {
            "triple": "host",
            "linker": {
                "arguments": [
                    "-lm",
                    "-lpthread"
                ]
            }
        }
    ]
}
```

See [DESIGN.md](DESIGN.md) for the full language specification and
[CONTRIBUTING.md](CONTRIBUTING.md) for build, test, and contribution details.
