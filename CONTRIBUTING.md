# Contributing to Zep

Thanks for working on Zep. This project is a C++26 compiler with modules, CMake target modules, and
a GoogleTest/CTest test suite. Keep changes small, tested, and aligned with the existing module
boundaries.

## Prerequisites

- CMake 3.30+
- Ninja
- Clang with C++26 module support
- LLVM development packages available to CMake

## Build

Configure and build the debug preset:

```bash
cmake --preset debug
cmake --build --preset debug
```

Build release when you need optimized binaries:

```bash
cmake --preset release
cmake --build --preset release
```

The debug build directory is `cmake-build-debug`; the release build directory is
`cmake-build-release`.

Build presets use two compilation jobs by default; override with `--parallel` when memory permits.
Release disables tests and avoids native CPU flags, forced LTO, and stripping. Enable
`BUILD_TESTING`, `ZEP_NATIVE`, or `ZEP_ENABLE_IPO` explicitly when required.

## Test

Run the default suite:

```bash
ctest --preset debug
```

List discovered tests:

```bash
ctest --test-dir cmake-build-debug -N
```

Snapshot tests compare tracked files by default. To intentionally rewrite snapshots:

```bash
ZEP_UPDATE_SNAPSHOTS=1 ctest --test-dir cmake-build-debug --output-on-failure
```

## Opt-in Tooling

Fuzzers, benchmarks, and coverage are available but are not part of normal `ctest`:

```bash
cmake --preset debug -DZEP_BUILD_FUZZERS=ON -DZEP_BUILD_BENCHMARKS=ON -DZEP_ENABLE_COVERAGE=ON
cmake --build cmake-build-debug --target zep_fuzz_smoke zep_benchmarks zep_coverage
```

- `ZEP_BUILD_FUZZERS=ON` enables libFuzzer targets and the `zep_fuzz_smoke` target.
- `ZEP_BUILD_BENCHMARKS=ON` enables Google Benchmark targets and the `zep_benchmarks` target.
- `ZEP_ENABLE_COVERAGE=ON` enables LLVM source-based coverage and the `zep_coverage` target.

## Compiler Options

Both project builds and single-file compilation accept optimization levels:

```bash
cmake-build-debug/cli/zep build -O 2
cmake-build-debug/cli/zep compile --input file.zep -O 3
```

Valid levels are `0`, `1`, `2`, and `3`. Level `0` is the default. Nonzero levels run LLVM module
optimization passes before object emission and select the matching LLVM codegen optimization level.

## Project Structure

```text
common/           Shared utilities: arena, logger, diagnostics, source, span, processes
frontend/         Lexer, parser, AST, semantic analysis, and debug dumpers
hir/              High-level IR node and program definitions
codegen/          Code generation interfaces and implementations
codegen/api/      Backend interface and generation options
codegen/llvm/     LLVM backend and LLVM-specific type/IR helpers
workspace/        Manifest, package graph, and environment model
compiler/         Module graph, semantic checking, HIR lowering, and specialization
builder/          Object generation, linking, and structured tool-process execution
lsp/              Language Server Protocol server implementation
cli/              Command-line interface (build, compile, lsp, fetch, install)
cmake/            Build helpers for modules, tests, fuzzing, benchmarks, and coverage
examples/pong/    Main multi-file Pong example project
```

The compiler pipeline is:

```text
Source -> Lexer -> Parser -> AST -> SemanticChecker -> Lowerer -> HIR -> Codegen -> LLVM IR
```

## Test Layout

Each CMake target module owns tests beside its `src/` directory:

```text
module/
  src/
  tests/
    CMakeLists.txt
    unit/
    support/
    fixtures/
    fuzz/
    benchmarks/
```

- Put default GoogleTest cases in `tests/unit`.
- Put test-only modules in `tests/support` and link them only into tests.
- Put source fixtures and snapshots in `tests/fixtures`.
- Use named `test "description" { ... }` blocks and `std.testing.Test` assertions for Zep runtime
  scenarios. Keep C++ tests for compiler internals, CLI behavior, process output, and isolated FFI
  overrides. The `zep test` command compiles once and executes each selected block in a fresh process.
- Put libFuzzer targets in `tests/fuzz`; they must be opt-in.
- Put Google Benchmark targets in `tests/benchmarks`; they must be opt-in.
- Copy mutable project fixtures into the CMake binary tree before running tests.
- Group tests by behavior in focused files, sharing setup through test support.
- Preserve distinct regressions; remove smoke tests already covered by stronger assertions.
- Consolidate small files within a subsystem. Keep separate files for independently built fuzzers,
  benchmarks, snapshots, and fixtures that require different process-wide FFI overrides.
- Keep a scenario at multiple pipeline stages only when each stage asserts its own contract;
  a parse-only duplicate adds no coverage to a test that already checks successful parsing and types.
- Use representative optimization levels; reserve full sweeps for assembly and ABI-sensitive cases.

Example frontend parser test:

```cpp
TEST(FrontendParser, ParsesStructDefaultsAndNestedTypes) {
    const std::string source =
        "struct Box { public: var value: i32 = 1 }";

    EXPECT_TRUE(assert_parse_ok(source));
}
```

Example type-checker test:

```cpp
TEST(FrontendTypeChecker, AcceptsNamedArguments) {
    const std::string source =
        "fn add(left: i32, right: i32) -> i32 { return left + right }\n"
        "fn main() -> i32 { return add(right: 2, left: 1) }\n";

    EXPECT_TRUE(assert_type_check_ok(source));
}
```

## Code Style

- Use `snake_case` for files, functions, variables, parameters, and fields.
- Use `PascalCase` for classes.
- Use wrapper enum classes with nested `enum class Type : std::uint8_t`.
- Prefer `auto` when the type is obvious, except function return types must be explicit.
- Do not use `auto` to create named instances such as `Parser parser`.
- Use classes, not structs.
- Keep class members ordered as `private`, `protected`, then `public`.
- Make externally used fields public rather than adding getter/setter wrappers.
- Check pointers explicitly with `== nullptr` or `!= nullptr`.
- Use explicit casts and explicit constructors.
- Reserve vectors before filling them in loops.
- Use `Logger::print` and `Logger::print_stderr` for console output.
- Keep comments rare; prefer clear code and focused tests.

## Contribution Checklist

1. Keep the change scoped to the module that owns the behavior.
2. Add or update tests beside that module. Put Zep tests in `.test.zep` files; keep standard-library
   tests under `std/tests/` and run them with `zep test --project std`.
3. Run `cmake --preset debug`.
4. Run `cmake --build --preset debug`.
5. Run `ctest --preset debug`.
6. Update docs when commands, layout, or workflows change.
