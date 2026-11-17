# Getting started

## Build the compiler

Install Make, CMake 3.30+, Ninja, Clang with C++26 module support, and LLVM development packages.
From the repository root:

```sh
make
make test
```

The compiler is `zep/cmake-build-debug/cli/zep`, relative to the repository root. Use `make release`
for an optimized build, or run CMake presets directly from `zep/`.
Make uses `nproc`; Ninja bounds memory-heavy compilation by RAM. Override `JOBS` when needed.

New build trees live inside `zep/`. Old root build trees contain paths from the previous layout;
leave them alone or remove them yourself, and configure a fresh tree rather than relocating a cache.

## Write a program

```zep
import std.io.print

public fn main() -> i32 {
    print("Hello, Zep!\n")

    return 0
}
```

For a project, place this in `src/main.zep` alongside a [project manifest](projects.md).
`print` is best effort and returns `void`; checked writes are available through the
[standard library](stdlib.md).

## Run the examples

The following commands start at the repository root:

```sh
zep/cmake-build-debug/cli/zep build --project examples/hello
examples/hello/build/hello
```

The [notes example](https://github.com/budchirp/zep-lang/tree/main/examples/notes) has separate HTTP server and CLI client projects,
named tests, and an in-memory database. Its README includes build and CRUD commands.

Pong uses raylib. Build its existing CMake project first if the library is not available:

```sh
cmake -S examples/pong/third_party/raylib -B examples/pong/third_party/raylib/build -DBUILD_EXAMPLES=OFF
cmake --build examples/pong/third_party/raylib/build --parallel "$(nproc)"
cd examples/pong
../../zep/cmake-build-debug/cli/zep build
./build/pong
```

Pong requires a graphical session. The notes server runs until interrupted. Examples are run
explicitly and are not launched by `make test`.
