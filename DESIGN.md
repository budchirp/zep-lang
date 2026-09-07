# Zep Language Design

## Lexical Structure

### Keywords

| Keyword | Usage |
|---------|-------|
| `fn` | Function declaration |
| `static` | Static struct method modifier |
| `override` | Instance method override marker |
| `struct` | Struct type declaration |
| `interface` | Interface type declaration |
| `enum` | Enum type declaration |
| `type` | Type alias declaration |
| `import` | Module import |
| `var` | Variable declaration (mutable or immutable) |
| `const` | Compile-time constant / const function qualifier (`fn f() const -> T`) |
| `mut` | Mutable binding (`var mut`), pointer (`*mut`), or method receiver qualifier (`fn m() mut -> T`) |
| `return` | Return statement |
| `defer` | Deferred statement, runs at scope exit |
| `do` | Block expression (`do { ... }`) |
| `if` / `else` | Conditional expression |
| `when` | Multi-branch conditional expression |
| `while` | Boolean loop statement |
| `for` / `in` | C-style and array range loop statements |
| `extern` | FFI declaration |
| `public` | Visibility modifier (exported) |
| `private` | Visibility modifier (module-local, default) |
| `true` / `false` | Boolean literals |
| `null` | Null pointer literal |
| `as` | Type cast operator |
| `is` | Type check operator |

### Operators

#### Binary

| Precedence | Operators | Result Type |
|------------|-----------|-------------|
| Assignment | `=` | Target type |
| Logical Or | `\|\|` | `boolean` |
| Logical And | `&&` | `boolean` |
| Equality | `==` `!=` | `boolean` |
| Comparison | `<` `>` `<=` `>=` | `boolean` |
| Type Check | `is` | `boolean` |
| Type Cast | `as` | Target type |
| Additive | `+` `-` | Numeric (left type) |
| Multiplicative | `*` `/` `%` | Numeric (left type) |

#### Unary

| Operator | Operand | Result Type |
|----------|---------|-------------|
| `+` | Numeric | Same as operand |
| `-` | Numeric | Same as operand |
| `!` | `boolean` | `boolean` |
| `*` | Pointer | Pointed-to type |
| `&` | Any | `*T` (immutable pointer) |
| `&mut` | Mutable lvalue | `*mut T` (mutable pointer) |

#### Postfix

| Operator | Operand | Result Type |
|----------|---------|-------------|
| `.` | Struct or enum value | Field or method |
| `->` | Pointer to struct or enum value | Dereference, then field or method |

### Literals

| Literal | Default Type | Context Type |
|---------|-------------|--------------|
| Integer (`42`) | `i32` | Any numeric type |
| Float (`3.14`) | `f64` | Any numeric type |
| String (`"hello"`) | `cstr` | - |
| Boolean (`true`/`false`) | `boolean` | - |

Escape sequences in strings: `\n`, `\r`, `\t`, `\\`, `\"`

## Types

### Primitives

| Type | Description |
|------|-------------|
| `void` | Unit type |
| `boolean` | Boolean |
| `char` | C character |
| `cstr` | C string |
| `any` | Top type, compatible with everything |
| `i8` `i16` `i32` `i64` | Signed integers |
| `u8` `u16` `u32` `u64` | Unsigned integers |
| `f32` `f64` | Floating-point |

Primitive values also have compiler-backed facade types for method lookup without changing their
runtime representation:

| Facade | Backing Type |
|--------|--------------|
| `Integer` | `i32` |
| `Float` | `f64` |
| `Boolean` | `boolean` |
| `Char` | `char` |
| `CStr` | `cstr` |

Facade methods use normal method syntax:

```
32.to_string()
true.to_string()
"zep".to_string()
```

`value.value` on a primitive facade returns the backing primitive value.

### Composite

| Syntax | Description |
|--------|-------------|
| `*T` | Immutable pointer |
| `*mut T` | Mutable pointer |
| `T[N]` | Fixed-size array |
| `T[]` | Unsized array |
| `(T1, T2) -> R` | Function/closure type |
| `Name` | Named type reference |
| `module.Name` | Type imported through a module alias |
| `Outer::Inner` | Nested named type reference |
| `Name<T>` | Generic type instantiation |
| `Name<Param: T>` | Named generic type instantiation |

### Type Compatibility

Compatibility is checked when assigning a value from a source type to a target type (variable init, argument pass, return, field write). The rule set is strict with a few explicit escape hatches.

Universal compatibility:
- `null` is compatible with any type (source or target).
- `any` is compatible with any type (source or target). It opts out of the strict check.
- `never` is compatible with any type (unreachable flow).

Pointer rules:
- A mutable target (`*mut T`) accepts only mutable sources. An immutable target (`*T`) accepts both.
- Element types must be compatible.
- `*void` accepts any pointer type, mutable or immutable.
- `*mut void` accepts only mutable pointers (`*mut T`).
- `*void` and `*mut void` do not implicitly convert back to `*T` or `*mut T`; use `as`.

Structural rules (both sides must have the same kind):
- Integers: same signedness and same size.
- Floats: same size.
- Booleans, strings, void: kind match.
- Arrays: same size, compatible elements.
- Structs: same registered name, compatible field types.
- Enums: same registered name, same variants in order, compatible payload fields.
- Interfaces: same registered name and compatible generic arguments.
- Named types: same name.
- Functions: compatible return type, parameter count match, pairwise compatible parameter types.

Inheritance compatibility:
- A derived struct value is compatible with any base struct in its base chain. Passing by value slices
  to the base fields and uses the base type's static methods.
- A pointer to a derived struct is compatible with a pointer to any base struct in its base chain.
- A struct value is compatible with an interface value only when the struct lists that interface and
  implements its required methods. Interface calls use dynamic dispatch through the interface value.

Numeric literals retain their default type (`i32` for integers, `f64` for floats) and do not implicitly widen. To use a literal in a different numeric context, use `as` or annotate.

## Declarations

### Variables

```
var name = expr              // immutable, type inferred
var name: Type = expr        // immutable, explicit type
var mut name = expr          // mutable, type inferred
var mut name: Type = expr    // mutable, explicit type
```

### Functions

```
fn name(params...) -> ReturnType { body }
fn name<T>(params...) -> ReturnType { body }    // generic
fn name<T: Constraint>(params...) -> ReturnType { body }
```

Parameters: `name: Type`

Call arguments can be positional or named. Positional arguments must come before named arguments:

```
name(1, y: 2)
name<T: i32, U: boolean>(value: 1, other: true)
```

Duplicate names, unknown names, missing required arguments, and positional arguments after named
arguments are errors.

### Structs

```
struct Name { members... }
struct Name<T> { members... }
struct Name<T: Constraint> { members... }
struct Derived : Base, Printable { members... }
```

```
struct Name {
    public:
        var field1: Type
        var field2: Type

    private:
        var internal: Type
}
```

Struct bodies may contain fields, methods, nested structs, and nested enums.

Visibility is controlled via `public:` and `private:` sections (not per-member modifiers):

```
struct Name {
    public:
        var field1: Type
        var field2: Type = default_value

    private:
        var internal: Type
}
```

Fields use the `var` keyword: `var name: Type [= default_value]`
- Fields with a default value may be omitted from struct literals
- `public:` section - fields and methods accessible outside the struct
- `private:` section (default) - inaccessible via member access or struct literals

    private:
        var x: i32

    public:
        var y: i32

        fn Name() -> Name { ... }
        fn Name(x: i32, y: i32) -> Name { ... }
        fn area() -> i32 { ... }
        fn move_by(dx: i32, dy: i32) mut -> void { ... }
        static fn origin() -> Name { ... }
        fn ~Name() -> void { ... }
}
```

Nested types are accessed via `Outer::Inner`:

```
struct Container {
    public:
        var value: i32

        struct Entry {
            public:
                var key: cstr
        }

        enum Kind {
            Small
            Large
        }
}
```

Rules:
- Instance methods receive `self` implicitly (no explicit `self` parameter)
- `self` is available as a variable inside methods: `self->method()` for method calls, bare name for field access
- `fn name(...) mut -> Ret` marks a method with mutable receiver (`*mut Self`). Regular `fn name(...) -> Ret` is immutable borrow (`*Self`).
- Static methods use `static fn` and are accessed with `Type::name`
- Constructors are `fn Type(...) -> Type` and may overload
- Destructors are `fn ~Type() -> void`
- `override fn` is required when a method matches a base method or listed interface requirement
- `value.name()` resolves an instance method call
- `Type::name()` resolves a static method call
- `Type(...)` resolves a constructor call
- `value.~Type()` explicitly destroys a local value
- Locals with destructors are destroyed automatically on scope exit and before `return`
- Nested types are registered with qualified names (e.g., `Container::Entry`)

Extension methods may be declared at module scope with a dotted function name:

```
fn Integer.to_x() -> i32 {
    return value + 1
}

fn Point.sum() -> i32 {
    return x + y
}
```

Extension methods are called like instance methods. They receive implicit `self` and do not
act as constructors, destructors, static methods, overrides, or interface implementations.

### Interfaces

```
interface Reader {
    public:
        fn read() -> *mut u8
}

interface ReadWriter : Reader {
    public:
        fn write(data: *u8) mut -> void
}
```

Interface bodies contain public method signatures only. Interfaces may inherit from other interfaces
by listing them in the inheritance list (after the `:`). Inherited interface methods are copied into
the derived interface.

Interface methods use implicit receiver. `fn name(...) mut -> Ret` specifies a mutable receiver (`*mut Self`) requirement;
regular `fn name(...) -> Ret` specifies an immutable receiver (`*Self`).

A struct satisfies an interface by listing it in the struct inheritance list and implementing each
required method.

### Enums

```
enum Color {
    Red
    Green
    Blue
}

enum Shape {
    Point
    Circle { radius: f64 }
    Rect { width: f64, height: f64 }
}

enum Option<T> {
    None
    Some { value: T }
}

enum Key : i32 {
    W = 87
    S = 83
}
```

Enum bodies use newline-separated members (no commas between variants). Variants may be unit
variants (no payload) or payload variants with named, typed fields inside braces. Variant
payload fields are comma-separated and may have default values: `field: Type = expr`.

Enums may declare a backing type with `enum Name : Type`. Integer-backed variants without
`= expr` start at zero or use the previous discriminant plus one. Other backing types require
explicit values. Discriminants must fit the backing type and be unique. Expressions may reference
earlier variants by name or through `Enum::Variant.value`; self and forward references are errors.
Backed variants cannot have payload fields. Backed enums
remain nominal; use `.value` to read the backing value:

```
var key = Key::W
var raw: i32 = key.value
```

Non-backed enums also support `.value`; it returns the active variant id as `i32`.

Enums support instance methods and static methods (no constructors or destructors):

```
enum Shape {
    Circle { radius: f64 }
    Rect { width: f64, height: f64 }

    fn area() -> f64 { ... }
    static fn unit_circle() -> Shape { ... }
}
```

Enum variants are constructed with `::` paths. Payload variants use named fields:

```
var color = Color::Red
var shape = Shape::Rect { width: 10.0, height: 20.0 }
var value = Option<T: i32>::Some { value: 42 }
```

Duplicate variant names and duplicate payload field names are errors. Payload construction must
provide every payload field exactly once with the correct type (unless the field has a default).

### Type Aliases

```
type Name = TargetType
type Name<T> = TargetType<T>
type Name<T: Constraint> = TargetType<T>
```

Creates a transparent type alias. The alias is fully compatible with its target type.

### Extern Declarations

```
extern fn name(params...) -> ReturnType
extern fn name(params..., ...) -> ReturnType
extern var name: Type
```

C variadic calls promote `boolean` and integer values narrower than `i32` to `i32`, and `f32`
to `f64`.

### Imports

```
import module.path
import module.path { symbol, symbol as local_name }
public import module.path
public import module.path { symbol, symbol as local_name }
```

Resolution:
- `import a` → `src/a.zep` or `src/a/index.zep`
- `import a.b` → `src/a/b.zep` or `src/a/b/index.zep`
- `import std.io` → global libs `~/.local/share/zep/libs/std/0.0.1/src/io/index.zep`

Plain imports bind a module alias using the final path segment: `import std.io` makes public
members available as `io.print(...)`. Explicit imports copy selected public symbols into the
current module: `import std.io { print, eprint as print_error }`. `public import` re-exports either the
module alias or the selected imported names.

The standard I/O module exports side-effecting `print` and `eprint` functions. `std.io.panic`
provides the `panic` function, while `std.io.format` provides `sprint`, `FormatArguments`, and
`FormatError`. Formatting uses sequential `{}` placeholders, `{{` and `}}` for literal braces, and
a concrete argument container that accepts strings, integers, floating-point values, booleans, and
characters. `sprint` returns a `Result` for missing or extra arguments and unmatched braces. `print`
and `eprint` terminate through `panic` on invalid formatting and otherwise return `void`.

## Expressions

### Block Expressions

```zep
var total = do {
    var a = 10
    var b = 20
    a + b
}
```

A `do { ... }` block expression evaluates a block of statements in a local lexical scope and
produces the value of the final statement or expression.

### When Expressions

```
when (subject) {
    value -> body,
    value1, value2 if (guard) -> body,
    Enum::Variant { field, other: renamed } -> body,
    else -> body,
}

when {
    condition -> body,
    else -> body,
}
```

Subject form evaluates the subject once and compares it to each arm condition in order.
Subjectless form evaluates each arm condition as a boolean expression. Arm guards must be
boolean and run only after the arm condition matches. Arm bodies can be a single expression or a
block; the expression value or final block statement becomes the arm value. All arm values must
have compatible types. An `else` arm is required and must be last.

V1 supports numeric, boolean, pointer-compatible subject matching, and enum variant matching with
named payload bindings. String subject matching, ranges, deeper destructuring, and exhaustiveness
without `else` are deferred.

### Loop Statements

```
while (condition) {
    body
}

for (var mut i: i32 = 0; i < 10; i = i + 1) {
    body
}

for (var item: i32 in items) {
    body
}

for (var i: i32 in 0..10) {
    body
}
```

Loop conditions must be `boolean`. C-style `for` loops require initializer, condition, and step
clauses; the initializer is either a `var` declaration or an expression. Range-form `for` accepts
fixed-size arrays `T[N]` or integer range expressions.

The binding in array form is an element copy; `var mut` makes the local binding reassignable but
does not write back to the array. The binding in range form is mutable by default (the loop counter).
Loops are statements with `void` type.

### Range Expressions

```
start..end                // integer range, exclusive end
```

`start` and `end` must be the same integer type. Ranges are only valid as iterables in
`for (var i in ...)`.

### Closures

```
{ param1, param2 -> body }
{ param1: Type, param2: Type -> body }
```

Parameter types inferred from target function type if available.

### Struct Literals

```
Name { field1: value1, field2: value2 }
Name<T> { field1: value1, field2: value2 }
```

All fields required. Private fields cannot be set. Duplicate fields are an error.

### Type Casts and Checks

```
expr as Type    // cast expression to type
expr is Type    // check if expression is of type (returns boolean)
```

## Visibility

| Modifier | Default? | Meaning |
|----------|---------|---------|
| `public` | No | Visible to importing modules |
| `private` | Yes | Module-local only |

Visibility applies to: functions, variables, structs, interfaces, enums, extern declarations, type aliases, imports.

Within struct/interface bodies, visibility is specified via `public:` and `private:` sections rather than
per-member modifiers. All members following the section marker inherit that visibility until the next
section marker or end of body.

Private struct fields cannot be accessed through member expressions or set in struct literals.

## Mutability

| Storage | Syntax | Reassignable | `&` | `&mut` |
|---------|--------|-------------|-----|--------|
| `var` | `var x = ...` | No | `*T` | Error |
| `var mut` | `var mut x = ...` | Yes | `*T` | `*mut T` |

Assignment requires a mutable lvalue: `var mut` variable, `*mut` dereference, or member/index on mutable root.

## Attributes

```
@mangle              // enable name mangling
@mangle(true)        // enable name mangling
@mangle(false)       // disable name mangling
@name("symbol")      // custom linker symbol
@os("linux")         // compile only on linux targets
@os("macos")         // compile only on macos targets
@os("linux", "macos") // compile on linux or macos
@arch("x86_64")      // compile only on x86_64 targets
@arch("aarch64")     // compile only on aarch64 targets
@section(".name")    // place global variable in a specific ELF/COFF/Mach-O section
@align(16)           // set alignment of a global variable (must be power of 2)
```

- `@mangle` cannot be used with `false` on generic or overloaded functions
- `@name` cannot be used with generic or overloaded functions
- `@os` and `@arch` accept one or more string arguments; the declaration is included if any argument matches the target
- `@section` requires a single string argument and applies to global variables
- `@align` requires a numeric argument and sets the alignment of a global variable

## Built-in Functions

```zep
#sizeof(T)           // returns the byte size of type T as i64
#length(array)       // returns the element count of a fixed-size array as i64
#asm("...")          // emits inline assembly string
```

- `#sizeof` evaluates at compile time and produces a constant `i64` value
- Applicable to all sized types except `void`
- `#length` evaluates at compile time for fixed-size arrays and produces a constant `i64` value
- `#asm` takes a string literal and emits inline target assembly

## Generics

```zep
fn identity<T>(value: T) -> T { return value }
struct Container<T> {
    public:
        var value: T
}
```

Generic constraints: `<T: Constraint>` — argument must be compatible with constraint type.

Generic instantiation: `identity<i32>(42)`, `Container<i32> { value: 42 }`

## Function Overloading

Multiple functions with the same name and different parameter types. Duplicate signatures produce a warning.

## Ownership and Cleanup

### Value and Reference Semantics

Values are passed by value (copy) by default. Pointers (`*T` for immutable, `*mut T` for mutable)
allow passing by reference. Memory allocation can be managed manually via the standard library
allocator or custom memory managers.

### Destructors

Structs declare a destructor as `fn ~Type() -> void`. Values with a destructor are cleaned up
automatically at scope exit and before `return`, in reverse order of declaration. Explicit
calls to `value.~Type()` are also permitted.

### `defer` Statement

```zep
defer <statement>
defer { statements... }
```

Registers a statement to run at scope exit. Deferred statements run in reverse registration
order (LIFO), interleaved with destructor calls from the same scope. Deferred statements run
on normal scope exit and upon early `return`.

## Project Configuration

Projects define a `zep.json` manifest at the project root:

```json
{
    "name": "project",
    "version": "0.1.0",
    "type": "executable",
    "libs": {
        "std": "0.0.1"
    },
    "target": [
        {
            "triple": "host",
            "linker": {
                "arguments": [
                    "-lm",
                    "-lpthread"
                ]
            }
        }
    ]
}
```

- `name`: package/project name
- `version`: semantic version string
- `type`: `"executable"` or `"library"`
- `libs`: dependencies mapped to a global version string (e.g. `"std": "0.0.1"`), a local path object (`{"version": "...", "path": "..."}`), or a git repository (`{"version": "...", "git": "..."}`)
- `target`: list of target compilation and linking configurations
- Entry point: `src/main.zep` for executables

## Compiler and Tooling Architecture

The compiler is organized into modular subsystems:

```text
common/           Shared utilities: arena, logger, diagnostics, source, span
frontend/         Compiler frontend
  ast/            AST node definitions and program root
  lexer/          Tokenizer
  parser/         Recursive descent parser with precedence climbing
  token/          Token types and keyword mappings
  sema/           Semantic analysis (type checker, scopes, resolvers, compile-time evaluator)
hir/              High-level IR representation, lowering, and monomorphization
codegen/          Code generation orchestration and LLVM backend
workspace/        Manifest reader, project model, package graph, toolchain discovery
compiler/         Compilation pipeline and module graph
builder/          Direct compiler, object generation, and linker orchestration
lsp/              Editor analysis, protocol transport, dispatch, sessions, codecs, and handlers
cli/              Command-line driver (build, compile, lsp, fetch, install)
```

### Compiler Pipeline

```text
Source → Lexer → Parser → AST → TypeChecker → HIRLowerer → HIR → Codegen → LLVM IR
```

### Language Server Protocol

The LSP is split into analysis, core, protocol, and handler modules. Documents apply incremental
UTF-16 edits and invalidate cached project snapshots through source overlays. LSP analysis owns one
shared syntax index, import-aware and lexical completion, diagnostics, hover, semantic tokens,
definitions, references, highlights, document and workspace symbols, and signature help. It invokes
the normal compiler entry point for project loading, parsing, semantic checking, imports, and source
overlays; it does not reimplement the compiler pipeline.

Completion supports local and inherited instance members, static access through `Type::member`, and
named call arguments. Named-argument entries insert the full `parameter: ` form and exclude arguments
already supplied at the call site.
