# Zep documentation

Zep compiles to native code through LLVM. Its resource model combines explicit allocation,
references, copy/move operations, deterministic destruction, and checked typed errors.

## Using the language

- [Getting started](getting_started.md): build the compiler and run the examples.
- [Lexical syntax](syntax.md): keywords, literals, and operators.
- [Types](types.md): primitive and composite types, conversions, and facade methods.
- [Declarations](declarations.md): variables, functions, structs, enums, and visibility.
- [Modules](modules.md): imports, exports, and source layout.
- [Expressions and control flow](expressions.md): blocks, matching, loops, and closures.
- [Generics and interfaces](generics.md): inference, constraints, and dynamic dispatch.
- [Ownership and memory](memory.md): references, special members, allocation, and cleanup.
- [Checked errors](errors.md): throwable structs, propagation, handlers, and callbacks.
- [Testing](testing.md): named tests, sibling access, and isolated execution.
- [Standard library](stdlib.md): text, collections, I/O, resources, and HTTP.
- [FFI and attributes](ffi.md): external declarations, ABI boundaries, and compiler attributes.
- [Projects and commands](projects.md): manifests, build options, and installation.
- [Editor support](editor.md): language-server features and extension configuration.

## Developing Zep

- [Compiler architecture](architecture.md): responsibilities, modules, and build organization.
- [Contributing](https://github.com/budchirp/zep-lang/blob/main/CONTRIBUTING.md)
- [Agent guidelines](https://github.com/budchirp/zep-lang/blob/main/AGENTS.md)
- [Priorities](https://github.com/budchirp/zep-lang/blob/main/zep/TODO.md)
