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
- Subdirs: `common`, `frontend` (with `ast/`, `lexer/`, `parser/`, `token/`, `arena/`, `sema/{checker,builder,resolver,type,scope}`, `debug/`), `hir` (sources only; not built by default), `codegen`, `driver`, `cli`

## Code Standards

### Naming Conventions

- **Files**: `snake_case`
- **Classes**: `PascalCase`
- **Enums/Enum Values**: `PascalCase`
- **Functions/Methods**: `snake_case`
- **Variables/Parameters/Fields**: `snake_case`

### Code Style

- DO NOT add comments to code. Keep code simple and clean.
- DO NOT add explicit types. use `auto` whenever you can. Function return types MUST be explicit no `auto` allowed there.
- 1. `private`, 2. `protected` and 3. `public` class order should be like this. DO NOT create methods like `get_x` and `set_x`.
- Use `auto` when the type can be deduced; DO NOT use `auto` when creating instances (e.g., `Parser parser`)
- DO NOT use shortcut names (e.g., `ty` for type). ONLY `ptr` for pointers is allowed.
- ALWAYS use classes (never structs); use explicit constructors to ensure all members are initialized.
- ALWAYS ASK QUESTIONS. Do not change code for unrelated issues; ask and I will guide you.
- Use modern C++ features. Use `Logger::print` / `Logger::print_stderr` for console output; do not use `std::print` or `std::println` outside the logger module.
- DO NOT use get_private_field and etc. If its gonna be used externally, make it public, if its used only within the class make it private.
- DO NOT use underscore postfix on field names (e.g. `context` not `context_`). Use same names for member and constructor parameter (e.g. `Foo(Context& context) : context(context)`).
- DO NOT use implicit conversions. Use explicit casts (e.g. `static_cast`, `explicit` constructors).
- ALWAYS check pointers explicitly with `== nullptr` or `!= nullptr`; do not rely on truthiness.
- KEEP CODE SPACIOUS: add blank lines between logical blocks.
- Add code examples to the plans that you make for lower intelligence model to understand
- Use `lldb` to debug code.
- DO NOT USE PYTHON OR BASH FOR REFACTORING.