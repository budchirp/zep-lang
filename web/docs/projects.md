# Projects and commands

Projects have a root `zep.json` and an executable entry point at `src/main.zep`:

```json
{
  "name": "project",
  "version": "0.1.0",
  "type": "executable",
  "libs": { "std": "0.0.1" },
  "target": [{ "triple": "host", "linker": { "arguments": ["-lm"] } }]
}
```

`type` is `executable` or `library`. Dependencies accept a global version, a local object with
`version` and `path`, or a git object with `version` and `git`. Paths are relative to the manifest.

| Command | Purpose |
|---|---|
| `zep build [--project path] [-O level] [--emit-ir]` | Build a project |
| `zep compile --input file [-o file] [--target triple] [-O level] [--emit-ir]` | Emit an object |
| `zep test [--project path] [--input file] [--filter text] [--list] [-O level]` | Run named tests |
| `zep fetch [--project path]` | Resolve dependencies |
| `zep lsp` | Start the language server |

Optimization levels are 0–3, defaulting to 0. Nonzero levels run LLVM optimization passes.

## Build and install Zep

```sh
make
make release
make test TEST_ARGS="-R CliHttp"
make install
```

Make accepts `PRESET`, `BUILD_DIR`, `PREFIX`, `JOBS`, `CMAKE_ARGS`, and `TEST_ARGS`.
It uses `nproc`; memory-heavy compilation is bounded by RAM. Debug retains line information;
set `ZEP_DEBUG_FULL_SYMBOLS=ON` through `CMAKE_ARGS` for full debugging.

Installation defaults to `~/.local/share/zep/bin/zep` and `~/.local/share/zep/libs/std/VERSION`.
Add the `bin` directory to PATH. Use `make install-zep` or `make install-std` separately.
`PREFIX` changes the destination and `DESTDIR` stages it. Std installation copies its manifest
and production sources; C++ compiler libraries are not installed. There is no CLI install command.

Package discovery checks `ZEP_LIBS_DIR`, the executable's sibling `libs/`, and the user's Zep libs.
Development builds use root `std/` as a fallback. See [contributing](https://github.com/budchirp/zep-lang/blob/main/CONTRIBUTING.md) for
direct CMake commands and optional tooling.
