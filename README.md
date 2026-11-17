# Zep

A systems language with generics, interfaces, payload enums, RAII, and checked typed errors.
Compiles to native code through LLVM.

```zep
import std.io.print

public fn main() -> i32 {
    print("Hello, Zep!\n")

    return 0
}
```

## Build and test

Requires Make, CMake 3.30+, Ninja, Clang with C++26 support, and LLVM development packages.

```sh
make
make test
make release
```

Compiler sources and build trees live in `zep/`. Make uses `nproc`; memory-heavy compilation is
limited by available RAM. Use `JOBS=N` to override parallelism.

## Install

```sh
make install
```

Installs the compiler to `~/.local/share/zep/bin/zep` and std to
`~/.local/share/zep/libs/std/0.0.1/`. Add `~/.local/share/zep/bin` to PATH.
Use `make install-zep` or `make install-std` separately, or set `PREFIX` for another location.

## Explore

- [Language documentation](web/docs/index.md)
- [Notes HTTP client/server example](examples/notes/README.md)
- [Building and running examples](web/docs/getting_started.md)
- [Contributing](CONTRIBUTING.md)
- [Priorities](zep/TODO.md)

Run the documentation site with `cd web && npm ci && npm run dev`.
