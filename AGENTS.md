# Zep Language - Agent Guidelines

## Building and Running

**Prerequisites:** CMake 3.30+, Ninja, Clang with C++26 support

**Configure and build:**
```
cmake --preset debug
cmake --build cmake-build-debug
```

**Run the compiler:**
```
./cmake-build-debug/cli/zep compile --input <source_file>
```

**Test with the design file:**
```
./cmake-build-debug/cli/zep compile --input examples/main.zep
```

**Release build:**
```
cmake --preset release
cmake --build cmake-build-release
```

## Project Structure

- C++26 with modules (`.cppm` extension)
- Build dir: `cmake-build-debug` (debug) or `cmake-build-release` (release)
- CMake modules are auto-discovered via `GLOB_RECURSE` from each module's `src/` directory

### Module Layout

```
common/           Shared utilities (arena, logger, diagnostics, source, span)
frontend/         Compiler frontend
  ast/            AST node definitions and program root
  lexer/          Tokenizer
  parser/         Recursive descent parser with precedence climbing
  token/          Token types and keyword mappings
  sema/           Semantic analysis
    type/         Type system (checker, builder, resolver, type definitions)
    scope/        Scope and symbol management
    resolver/     Call, generic, and struct resolution
  debug/          AST and sema dumpers
hir/              High-level IR
  hir/            HIR node and program definitions
  debug/          HIR dumper
  builder.cppm    Visitor that lowers AST to HIR
  lowerer.cppm    Type lowering and monomorphization helpers (templated)
  monomorphizer.cppm  Generic specialization cache and mangling
codegen/          Code generation orchestrator
  common/         Abstract driver interface (CodegenDriver, Backend enum)
  llvm/           LLVM backend (isolated; LLVM find_package lives here)
    codegen.cppm  LLVMCodegen visitor (inherits CodegenDriver + HIRVisitor)
    context.cppm  LLVMCodegenContext (llvm::LLVMContext, IRBuilder, Module)
    helper.cppm   LLVMCodegenHelper (type mapping, function/struct declarations)
    scope.cppm    CodegenScope (LLVM value scope stack)
  codegen.cppm    Codegen orchestrator (selects backend via Backend::Type)
driver/           Compiler driver (pipeline orchestration)
cli/              Command-line interface (argparse, compile command)
```

### Compiler Pipeline

`Source → Lexer → Parser → AST → TypeChecker → HIRBuilder → HIR → Codegen → LLVM IR`

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
- Use `lldb` to debug code.
- DO NOT USE PYTHON OR BASH FOR REFACTORING. Make changes manually through code editing tools.