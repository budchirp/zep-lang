# Zep

Systems programming language with manual memory management, generics, interfaces, and zero-cost abstractions. Compiles to native code via LLVM.

## Features

- **RAII** — recursive copies, move constructors, assignment special members, destructors, and deterministic `defer`
- **References** — `&T`, `&mut T`, and `move T`, with std `Memory::move(value)` for move construction and assignment
- **Generics** — `fn identity<T>(value: T) -> T`, `struct Container<T>`
- **Interfaces** — structural contracts with dynamic dispatch
- **Enums with payloads** — `Option<T>::Some { value: T }`, `Result<T, E>::Ok { value: T }`, or backed variants (`enum Key : i32`)
- **Struct inheritance** — single inheritance with `override fn`
- **defer** — scope-exit cleanup in reverse order
- **when** — multi-branch matching with guards and destructuring
- **Block expressions** — `do { ... }` blocks that evaluate to expressions
- **Primitive facades** — `32.to_string()`, `"zep".to_string()`, `true.to_string()`
- **C FFI** — `extern fn`, `extern var` with variadic support
- **Memory** — explicit allocation, placement `new (storage) value`, and `value.~Type()` destruction
- **Built-ins** — `#sizeof`, `#length`, and named-operand `asm { ... }` blocks
- **Language Server Protocol** — incremental document sync, import-aware completion, navigation,
  symbols, signatures, diagnostics, hover, and semantic tokens through `zep lsp`

## Example

```zep
import std.io.FormatArguments
import std.io.print

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
    var mut arguments = FormatArguments()
    arguments.add(point.distance_squared())
    print("distance squared: {}\n", arguments).unwrap()

    var circle = Circle(5.0)
    arguments.clear()
    arguments.add(circle.area())
    print("area: {}\n", arguments).unwrap()

    return 0
}
```

## Build

Prerequisites: CMake 3.30+, Ninja, Clang with C++26 support

```bash
cmake --preset debug
cmake --build --preset debug
```

The debug preset builds tests. Release builds omit tests by default and produce portable,
unstripped binaries. Enable `BUILD_TESTING`, `ZEP_NATIVE`, or `ZEP_ENABLE_IPO` explicitly when needed.
Installation includes the compiler and std library, excluding third-party headers and test libraries.
Build presets limit parallel compilation to two jobs to keep C++ module builds within reasonable
memory use; override with `--parallel`. Existing compiler launchers and linker selections are respected.

Build configuration lives in focused files under `cmake/`: `Options`, `Compiler`, `Dependencies`,
`Modules`, `Testing`, and `Install`, with separate opt-in sanitizer and instrumentation support.

## Test

```bash
ctest --preset debug
```

The test preset runs two tests at a time and reports failures. Assembly and callback ABI tests use
all four optimization levels; other runtime fixtures use representative levels rather than a full sweep.

Use `ctest --test-dir cmake-build-debug -N` to list the currently discovered tests.

Put named tests in `.test.zep` files, beside the source or under `tests/`:

```zep
import std.testing.Test

test "addition works" {
    Test::expect(2 + 3 == 5)
}
```

```bash
cmake-build-debug/cli/zep test
cmake-build-debug/cli/zep test --filter "addition" -O 2
cmake-build-debug/cli/zep test --list
cmake-build-debug/cli/zep test --input path/to/file.test.zep
cmake-build-debug/cli/zep test --project std
```

Project discovery includes `.test.zep` files under `src/` and `tests/`. `src/math.test.zep`
and `tests/math.test.zep` can access private declarations from `src/math.zep`. Ordinary builds
exclude test files; explicitly loaded tests are always type-checked. Standard-library tests live
under `std/tests/`; `tests/fixtures/` holds specialized subprocess inputs and is excluded from
discovery. Each selected test runs in a fresh process, so assertion
failures, panics, and crashes do not stop subsequent tests. The command reports totals and returns
a failure status when any test fails or no tests match. Filesystem state is shared between tests.

## Optimize

Use `-O` with levels `0`, `1`, `2`, or `3`:

```bash
cmake-build-debug/cli/zep build -O 2
cmake-build-debug/cli/zep compile --input path/to/file.zep -O 3
```

Level `0` is the default. Higher levels run LLVM optimization passes before object emission.

## Run

Run the Pong example project:

```bash
cd examples/pong
../../cmake-build-debug/cli/zep build
./build/pong
```

Pong uses scoped `Window` and `Frame` owners for automatic cleanup, a color-backed palette enum,
borrowed drawing arguments, and inferred std math calls.

Compile a single source file directly:

```bash
cmake-build-debug/cli/zep compile --input examples/pong/src/main.zep --output build/main.o
```

Launch the Language Server:

```bash
cmake-build-debug/cli/zep lsp
```

## CLI Commands

- `zep build [--project <path>] [-O|--optimization 0|1|2|3] [--emit-ir]`: Build a project defined by `zep.json`
- `zep compile --input <file> [-o|--output <file>] [--target <triple>] [-O|--optimization 0|1|2|3] [--emit-ir]`: Compile a source file to an object file
- `zep lsp`: Start the Language Server Protocol server (stdin/stdout)
- `zep fetch [--project <path>]`: Download and resolve package dependencies
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

See [TODO.md](TODO.md) for unfinished compiler work and prioritized language additions.
The [assembly example](examples/assembly/src/main.zep) demonstrates named inputs, multiple register
constraints, and a Linux x86_64 syscall.
