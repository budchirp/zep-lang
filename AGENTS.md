# Zep agent guidelines

## Workflow

- Ask questions before changes when requirements are unclear. Do not fix unrelated issues.
- Follow the user's existing decisions; do not ask again for authorized work.
- Preserve user changes and artifacts. Update `zep/TODO.md` as work progresses.
- Make source edits manually through editing tools. Do not use Python or Bash refactoring scripts.
- Include concrete code examples in implementation plans.
- Use GDB or LLDB to investigate runtime failures.

## Build and validation

Requires Make, CMake 3.30+, Ninja, Clang with C++26 modules, and LLVM development packages.
Run from the repository root:

```sh
make
make test
make test TEST_ARGS="-R CliHttp"
make release
make install
```

Builds use `zep/cmake-build-PRESET`, `nproc`, and RAM-bounded Ninja compilation pools. Run only
one build per build tree. Use fresh trees after source relocation; preserve old root caches.
Debug defaults to line-table symbols; use `CMAKE_ARGS="-DZEP_DEBUG_FULL_SYMBOLS=ON"` when needed.
Full CMake presets remain available from `zep/`. Installation belongs to Make/CMake and defaults
to `~/.local/share/zep/{bin,libs}`.

Run affected tests, then the full suite. Keep snapshots unchanged unless an intentional update is
explicitly enabled with `ZEP_UPDATE_SNAPSHOTS=1`. Keep normal tests deterministic and fast.
Fuzzing, benchmarks, and coverage are opt-in; see [CONTRIBUTING.md](CONTRIBUTING.md).

Build examples with `zep/cmake-build-debug/cli/zep build --project examples/hello` and similarly
for notes client/server. Pong builds from `examples/pong/` with
`../../zep/cmake-build-debug/cli/zep build`; use bounded startup/shutdown checks for interactive apps.

## Architecture

- Compiler components and CMake live in `zep/`; std, examples, and extensions stay at root.
- VitePress lives in `web/`, with Markdown pages in `web/docs/`. Run `npm run build` from `web/`.
- C++ production implementations belong inside classes in `.cppm` modules.
- Keep component-local `src/` and `tests/`; CMake discovers sources from each `src/` directory.
- Keep semantic decisions in frontend checking/resolution and consume them during HIR lowering.
- Keep LLVM isolated in `zep/codegen/llvm/`; retain public compiler/codegen module namespaces.
- Build driver code belongs to compiler. HIR definitions and lowering live in `zep/lowerer/`
  and use `zep.lowerer` modules.
- Keep scope/environment management separate from symbol definitions.
- Give each policy one implementation. Prefer cohesive modules over tiny forwarding classes.
- Use diagnostics and Logger for compiler failures, not C++ throws for source/pipeline errors.
- Use shared test support, explicit fixture roots, and build-tree copies of mutable projects.
- Put Zep tests in `.test.zep` files and std tests in `std/tests/`.

Pipeline: `Source → Lexer → Parser → AST → SemanticChecker → Lowerer → HIR → Codegen → LLVM IR`.
See [architecture](web/docs/architecture.md) and [language documentation](web/docs/index.md).

## Naming and style

- Use `snake_case` for files, functions, variables, parameters, and fields; `PascalCase` for classes.
- Use plural subsystem names and singular entity names. Keep names complete; only `ptr` may abbreviate.
- Enums use a wrapper class with nested `enum class Type : std::uint8_t`; values use `PascalCase`.
- Always use classes, never structs. Initialize members explicitly through constructors.
- Order access sections `private`, `protected`, then `public`.
- Make externally used fields public; do not add getter/setter wrappers.
- Do not suffix fields with underscores. Match constructor parameter names to fields.
- Use `auto` when deducible, except named class instances and function return types.
- Use explicit casts and explicit constructors; do not rely on implicit conversions.
- Check pointers with `== nullptr` or `!= nullptr`.
- Reserve vectors before filling them in loops.
- Add blank lines between logical steps. Expand cramped branches and keep code vertically spacious.
- Remove dead code, redundant variables, repeated logic, and trivial single-use forwarding helpers.
- Add no code comments. Use clear names and focused operations.
- Use modern C++ features where they simplify code.
- Use `Logger::print` and `Logger::print_stderr`; do not use `std::print` outside Logger.
