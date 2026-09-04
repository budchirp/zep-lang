# Zep Language - Agent Guidelines

## Building, Running, and Testing

**Prerequisites:** CMake 3.30+, Ninja, Clang with C++26 support

**Configure and build:**
```bash
cmake --preset debug
cmake --build cmake-build-debug
```

**Run the default test suite:**
```bash
ctest --test-dir cmake-build-debug --output-on-failure
ctest --test-dir cmake-build-debug -N
```

**Test with the Pong example (multi-file project with imports and FFI):**
```bash
cd examples/pong
../../cmake-build-debug/cli/zep build
./build/pong
```

**Release build:**
```bash
cmake --preset release
cmake --build cmake-build-release
```

**Opt-in heavy test tooling:**
```bash
cmake --preset debug -DZEP_BUILD_FUZZERS=ON -DZEP_BUILD_BENCHMARKS=ON -DZEP_ENABLE_COVERAGE=ON
cmake --build cmake-build-debug --target zep_fuzz_smoke zep_benchmarks zep_coverage
```

Fuzzers, benchmarks, and coverage are not part of normal `ctest`. Snapshot tests compare tracked
snapshots by default; set `ZEP_UPDATE_SNAPSHOTS=1` only when intentionally updating snapshots.

**Compiler optimization levels:**
```bash
cmake-build-debug/cli/zep build -o 2
cmake-build-debug/cli/zep compile --input file.zep -o 3
```

Valid optimization levels are `0`, `1`, `2`, and `3`. The selected level is passed through the
driver to LLVM target-machine code generation and LLVM module optimization passes.

## Project Structure

- C++26 with modules (`.cppm` extension)
- Build dir: `cmake-build-debug` (debug) or `cmake-build-release` (release)
- CMake modules are auto-discovered via `GLOB_RECURSE` from each module's `src/` directory
- CMake target modules own sibling `tests/` directories when they have tests

### Module Layout

```
common/           Shared utilities (arena, logger, diagnostics, source, span)
  tests/          Unit tests plus shared zep_test_support helpers
frontend/         Compiler frontend
  ast/            AST node definitions and program root
  lexer/          Tokenizer
  parser/         Recursive descent parser with precedence climbing
  token/          Token types and keyword mappings
  sema/           Semantic analysis
    type/         Type system (checker, builder, resolver, type definitions)
    scope/        Scope and symbol management
    resolver/     Call, generic, enum, struct, attribute, builtin, when, member resolution
    const/        Compile-time evaluation and constant values
    declaration/  Declaration checking
  debug/          AST, Sema, and Type dumpers
  tests/          Lexer, parser, type compatibility, and semantic tests
hir/              High-level IR
  hir/            HIR node and program definitions
  debug/          HIR dumper
  builder.cppm    Visitor that lowers AST to HIR
  lowerer.cppm    Type lowering and monomorphization helpers (templated)
  monomorphizer.cppm  Generic specialization cache and mangling
  tests/          HIR lowering and monomorphization tests
codegen/          Code generation orchestrator
  common/         Abstract driver interface (CodegenDriver, Backend enum)
  llvm/           LLVM backend (isolated; LLVM find_package lives here)
    codegen.cppm  LLVMCodegen visitor (inherits CodegenDriver + HIRVisitor)
    context.cppm  LLVMCodegenContext (llvm::LLVMContext, IRBuilder, Module)
    helper.cppm   LLVMCodegenHelper (type mapping, function/struct declarations)
    scope.cppm    CodegenScope (LLVM value scope stack)
  codegen.cppm    Codegen orchestrator (selects backend via Backend::Type)
  tests/          Codegen orchestration tests
workspace/        Manifest, Project model, package graph, and environment discovery
compiler/         Module graph, analysis service, parsing, semantic checking, and HIR lowering
  module/         Module metadata, graph loading, and import resolution
builder/          Pure build plans and structured tool-process execution
  tests/          Project config, module loading, and object compilation tests
lsp/              Language Server Protocol server implementation
  analysis/       AST/scope completion, hover, and semantic tokens extraction
  protocol/       LSP types, JSON-RPC framing, and transport
  server/         Server dispatcher and document management
  features/       Lifecycle, hover, completion, diagnostics, tokens handlers
  tests/          LSP unit and integration tests
cli/              Command-line interface (zep build, compile, lsp, fetch, install)
  tests/          CLI fixture and failure-mode tests
cmake/            Build helpers for modules, tests, fuzzing, benchmarks, and coverage
```

### Test Layout

Each CMake target module may contain:

```
module/
  src/            Production module sources
  tests/
    CMakeLists.txt
    unit/         Default GoogleTest/CTest tests
    support/      Optional test-only modules linked only into tests
    fixtures/     Input projects, source snippets, and snapshots
    fuzz/         Opt-in libFuzzer targets
    benchmarks/   Opt-in Google Benchmark targets
```

Default tests must stay fast and deterministic. Buildable project fixtures are copied into the
CMake binary tree before mutation. Do not rewrite tracked snapshots unless
`ZEP_UPDATE_SNAPSHOTS=1` is set.

### Compiler Pipeline

`Source → Lexer → Parser → AST → TypeChecker → HIRLowerer → HIR → Codegen → LLVM IR`

## Code Standards

### Naming Conventions

- **Files**: `snake_case`
- **Classes**: `PascalCase`
- **Enums**: Wrapper class with nested `enum class Type : std::uint8_t { ... }` (e.g., `Backend::Type::LLVM`, `Linkage::Type::External`)
- **Enum Values**: `PascalCase`
- **Functions/Methods**: `snake_case`
- **Variables/Parameters/Fields**: `snake_case`

### Code Style

- DO NOT add comments to code. Keep code simple and clean.
- DO NOT add explicit types when the type can be deduced; use `auto` whenever possible. Function return types MUST be explicit — no `auto` allowed there. DO NOT use `auto` when creating instances (e.g., `Parser parser`).
- Class member order: 1. `private`, 2. `protected`, 3. `public`. DO NOT create getter/setter methods like `get_x` and `set_x`. If a field is used externally, make it `public`; if only within the class, make it `private`.
- DO NOT use shortcut names (e.g., `ty` for type, `decl` for declaration). ONLY `ptr` for pointers is allowed.
- DO NOT use underscore postfix on field names (e.g., `context` not `context_`). Use same names for member and constructor parameter (e.g., `Foo(Context& context) : context(context)`).
- DO NOT use implicit conversions. Use explicit casts (e.g., `static_cast`, `explicit` constructors).
- ALWAYS use classes (never structs); use explicit constructors to ensure all members are initialized.
- ALWAYS check pointers explicitly with `== nullptr` or `!= nullptr`; do not rely on truthiness.
- ALWAYS `reserve()` vectors before populating them in loops to avoid unnecessary reallocations.
- KEEP CODE SPACIOUS: add blank lines between logical blocks.
- Use modern C++ features (structured bindings, init-statements in `if`, `std::views`, etc.).
- Use `Logger::print` / `Logger::print_stderr` for console output; do not use `std::print` or `std::println` outside the logger module.

### Architecture Rules

- ALWAYS ASK QUESTIONS before making changes. Do not change code for unrelated issues; ask and I will guide you.
- Add code examples to the plans that you make for lower intelligence models to understand.
- Use `lldb` or `gdb` to debug code.
- DO NOT USE PYTHON OR BASH FOR REFACTORING. Make changes manually through code editing tools.

## Language Design

See [DESIGN.md](DESIGN.md) for the complete language specification including types, syntax, semantics, visibility, mutability, generics, and all language features.
