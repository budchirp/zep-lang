# Contributing

## Build and test

Install Make, CMake 3.30+, Ninja, Clang with C++26 module support, and LLVM development packages.
From the repository root:

```sh
make
make test
make test TEST_ARGS="-R CliHttp"
make release
```

Builds live in `zep/cmake-build-PRESET`. Make uses `nproc`; Ninja bounds compilation by RAM.
Override `JOBS`, `PRESET`, `BUILD_DIR`, `CMAKE_ARGS`, or `TEST_ARGS` when needed.
Debug uses line-table symbols; enable `-DZEP_DEBUG_FULL_SYMBOLS=ON` for variable-level debugging.
Ccache is opt-in because C++ module artifacts require special handling. Release disables tests
and does not force native CPU flags, LTO, or stripping.

CMake can also be used directly from `zep/`:

```sh
cmake --preset debug
cmake --build --preset debug --parallel "$(nproc)"
ctest --preset debug
```

Use fresh build trees after moving sources. Do not relocate old CMake caches.

## Organization

- `zep/`: compiler components, CMake helpers, and active TODO.
- `std/`: standard library and its `.test.zep` tests.
- `examples/` and `extensions/`: sample applications and editor integration.
- `docs/`: concise, framework-neutral MDX documentation for the future website.

See [architecture](docs/architecture.mdx) for component responsibilities and
[AGENTS.md](AGENTS.md) for code standards. Keep public module namespaces unchanged.

## Tests

Keep C++ contract tests beside their component in `tests/unit/`, shared test-only modules in
`tests/support/`, and inputs/snapshots in `tests/fixtures/`. Copy mutable fixtures into build-tree
workspaces. Use `.test.zep` files for language/runtime scenarios, and keep std tests under `std/tests/`.
Preserve distinct regressions; avoid duplicate smoke checks and repeated optimization sweeps.

Snapshots are compared by default. Set `ZEP_UPDATE_SNAPSHOTS=1` only for an intentional update.
Use representative optimization levels; reserve complete sweeps for ABI-sensitive coverage.
Run examples explicitly; `make test` does not launch interactive programs or servers.

Heavy tooling is opt-in:

```sh
make build CMAKE_ARGS="-DZEP_BUILD_FUZZERS=ON -DZEP_BUILD_BENCHMARKS=ON -DZEP_ENABLE_COVERAGE=ON"
cmake --build zep/cmake-build-debug --parallel "$(nproc)" --target zep_fuzz_smoke zep_benchmarks zep_coverage
```

## Changes

Keep each behavior in its owning subsystem, share policy, and avoid compatibility aliases.
Add focused regressions, run affected tests and the full suite, and use GDB or LLDB for runtime
failures. Update relevant MDX documentation and `zep/TODO.md` alongside implementation.

Use `make install` for compiler/std installation, defaulting to `~/.local/share/zep/{bin,libs}`.
Use `PREFIX` or `DESTDIR` for custom or staged installation. Compiler C++ libraries are not installed.
