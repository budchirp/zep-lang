# Zep Language Design

## Lexical Structure

### Keywords

| Keyword | Usage |
|---------|-------|
| `fn` | Function declaration |
| `test` | Named module-level test declaration |
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
| `void` | No returned value |
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
| `&T` | Borrowed immutable reference |
| `&mut T` | Borrowed mutable reference |
| `move T` | Reference accepting a temporary or an explicit move |
| `T[N]` | Fixed-size array |
| `T[]` | Unsized array |
| `(T1, T2) -> R` | Function/closure type |
| `Name` | Named type reference |
| `module.Name` | Qualified named type reference |
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
- Array pointees are compared with their complete shape before considering decay. Array aliases
  follow the same rules as their underlying types, including in generic and indirect calls.
- A source pointer to an array can decay by one array dimension to a compatible element pointer.
  The reverse conversion requires `as`; a pointer to one element does not establish array storage.
- Pointer overloads prefer matching pointees over decay or void-pointer conversion. Among matches
  with the same pointee type, matching qualifiers beat adding immutability to a mutable pointer.
- Implicit C-string conversion accepts byte pointers and pointers to character or `u8` arrays,
  not pointers to arrays of other element types. The caller supplies null-terminated storage.

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

Without a numeric expected type, literals default to `i32` for integers and `f64` for floats.
A concrete numeric expected type contextualizes the literal; already-typed numeric values do not
implicitly widen. Use `as` for explicit numeric conversions.

## Declarations

### Named tests

```zep
import std.testing.Test

fn add(left: i32, right: i32) -> i32 {
    return left + right
}

test "addition preserves the result" {
    Test::expect(add(2, 3) == 5)
    Test::expect(add(-1, 1) == 0, "opposite values should cancel")
}
```

Test declarations require `.test.zep` files and are module-level, order-independent declarations
with a nonempty string name.
Names must be unique within a module; separate modules may use the same name. Tests have access
to private declarations in their own module and its production sibling. `src/math.test.zep` and
`tests/math.test.zep` both borrow the declaration scope of `src/math.zep`; test declarations and
imports remain in their own scope and do not modify the production module. Mirrored nested paths
follow the same rule, including `index.test.zep` paired with `index.zep`. A file without a production
sibling is an independent test module. Their bodies follow ordinary zero-argument `void` function rules,
including scope cleanup and bare `return`. Visibility modifiers, attributes, parameters, value-bearing
returns, and tests inside functions or types are invalid. Test names do not create callable bindings.

All loaded test bodies are type-checked, including explicitly compiled files and language-server
analysis. Ordinary builds do not discover test files and omit test functions if explicitly loaded.
`zep test` loads root-package source modules and discovers `.test.zep` files under `src/` and `tests/`.
The `tests/fixtures/` directories contain specialized subprocess inputs and are not discovered.
Production module imports retain the existing file/index resolution rules. Dependencies' test files
are not discovered. `--input` restricts discovery to the selected file and its imports and sibling,
using its enclosing project when present and standalone resolution otherwise.
Standard-library scenarios live under `std/tests/` and run with `zep test --project std`.

Tests are ordered by module path and source declaration order. `--filter` matches a substring of
the qualified display name; `--list` checks and lists tests without generating objects. An empty
selection is an error. The command generates one host executable under `build/tests/`, invokes it
separately for each selected test, prints results and totals, and fails if any test fails.

Each test starts with fresh process globals and uses the project directory as its working directory.
Filesystem state is shared. Ordinary scope and global cleanup run on normal completion.
`Test::expect(condition)` and `Test::expect(condition, message)` fail through `Test::fail(message)`,
which calls the allocation-free panic implementation. Messages are C strings. Assertion failures
terminate the child process without unwinding; the runner continues with the remaining tests and
reports nonzero exits and signals. Application entry points remain callable under generated names.

The first implementation runs sequentially on Linux x86_64 hosts. It does not add test hooks,
timeouts, skipping, parallel execution, or automatic assertion-expression locations.

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
- A constructor whose only parameter is `source: &Type` is the copy constructor
- Destructors are `fn ~Type() -> void`
- `override fn` is required when a method matches a base method or listed interface requirement
- `value.name()` resolves an instance method call
- `Type::name()` resolves a static method call
- `Type(...)` resolves a constructor call
- `value.~Type()` explicitly destroys a local value
- Owned values are destroyed automatically on scope exit and before `return`
- Copy construction is used for lvalue initialization, by-value arguments, and lvalue returns
- The compiler elides copies when a value is constructed directly into its destination
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

Shared fields can be declared once in an enum header:

```zep
enum FileError(error: IoError) {
    Open
    Read
    Write
}

var error = FileError::Read { error: IoError::NotFound }
var cause = error.error
```

Shared fields are public and exist in every variant. They can coexist with variant payloads and
generic parameters. Patterns may bind shared fields or omit them. Shared fields initialize before
the active payload and are destroyed after it, in reverse declaration order. Backed enums cannot
have shared fields.

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
import module.path.symbol
import module.path.symbol as local_name
```

Resolution:
- `import a.symbol` loads `src/a.zep` or `src/a/index.zep`
- `import a.b.symbol` loads `src/a/b.zep` or `src/a/b/index.zep`
- `import std.io.print` loads the installed standard library's `io/index.zep`

Imports name one exported symbol, optionally renamed with `as`. Imported public symbols retain
their visibility, allowing index modules to re-export canonical names such as `std.text.String`.
Whole-module aliases, brace-list imports, and `public import` are not implemented.

The standard I/O module exports side-effecting `print` and `eprint` functions. `std.io.panic`
provides the `panic` function, while `std.io.format` provides `format`, `FormatArguments`, and
`FormatError`. Formatting uses sequential `{}` placeholders, `{{` and `}}` for literal braces, and
a concrete argument container that accepts strings, integers, floating-point values, booleans, and
characters. Argument containers own their text values. `format` returns a `Result` for missing or
extra arguments, unmatched braces, and numeric conversion failures. It requires an explicit argument
container. `print(value)` and `eprint(value)` write plain text, including literal braces;
`print(pattern, arguments)` and `eprint(pattern, arguments)` format it. Both forms return
`Result<i64, PrintError>`. Patterns and text accept `cstr`, `String`, and `StringView`; bounded forms
preserve embedded zero bytes. Panic writes directly to standard error without formatting or
allocation, with its low-level write loop inline in `panic`.

Canonical exports are available from `std.core`, `std.collections`, `std.text`, `std.io`, `std.fs`,
`std.net.socket`, and `std.net.http`. Collections, strings, views, and array facades use `length()`
with `i64` counts and indices. String capacity counts usable bytes and excludes its zero terminator.

`String` and `Vector<T>` own their storage and copy it deeply. `StringView` borrows a
bounded byte sequence; constructing a `String` from a view preserves embedded zero bytes. `Box<T>`
and resource handles are move-only. `Memory::move(box).into_inner()` extracts the initialized boxed value.
`Box::from_raw` adopts compatible allocated storage; `into_raw()` transfers it to the caller.
Vectors destroy removed elements and move initialized elements during growth. `truncate()` destroys
the removed tail; `resize()` copies a fill value into newly initialized elements. Public `reserve()`,
`resize()`, `truncate()`, and string `repeat()` return results for invalid sizes and overflow.
`Vector::with_capacity()` replaces the capacity constructor and returns a result.
`Map<K, V>` stores
pairs in one vector and uses its supplied equality function for linear lookup; it is not a hash map.
Map keys and algorithm inputs use references; comparison callbacks also receive references.

String/view arguments and formatting containers use references, for example `text.append(view)`
and `format("{}", arguments)`. Construct C-string copies with `String(value)`; numeric conversions
use `String::from(value)`. Integer conversion uses one stack buffer and one owned allocation;
floating-point conversion returns `Result<String, ConversionError>`. Formatting arguments store owned
text or a conversion error when added; formatting reports errors with their argument index.
`arguments.append_to(index, output)` appends a stored argument directly and returns a result.
Generic calls use their arguments and expected result type for inference:
`var values: Vector<String> = Vector()` and `return Result::Ok { value: value }`.
An unconstrained local factory call still needs a type annotation. Typed argument bindings take
precedence over result context.

`std.ffi.handle.Handle`, `std.ffi.pipe.Pipe`, and `std.ffi.directory.DirectoryHandle` own OS resources
and close them on destruction. Moving transfers ownership and leaves the source closed. Explicit
`close()` reports failures and is safe to repeat. `File` owns a handle, and `Directory` owns a directory
handle. `File::borrow(descriptor)` borrows an existing descriptor; `File::socket(descriptor)` owns a
socket and uses a socket-safe write operation. `TcpStream` exposes its `file` for ordinary I/O.
`File::write()` owns the full-write policy, while the FFI handle owns interrupted-operation retry.
Console, file, socket, HTTP, and process code reuse these operations.

`std.fs` exports `File`, `FileMode`, `SeekOrigin`, `Directory`, `DirectoryEntry`, `Path`, and `FileError`.
File byte reads return `Vector<u8>` and stream until EOF without requiring a
seekable input. `Directory::open` supports incremental `next()` reads; `Directory::list` collects
owned names. Directory creation lives on `Directory::create` and `create_all`; existence checks live
on `Path::exists`. `IoError` has named OS error values and preserves unknown numeric codes through
its backing value. `FileError` and `ProcessError` are payload enums identifying the failed operation
and its I/O error. Typed path and command overloads reject embedded zero bytes at the C boundary.
`std.io.console.StandardFile` borrows standard output/error descriptors without closing them.
`Process::capture` reads through a borrowed `File` and uses the pipe owner to close and wait. It
preserves binary output. Process exit codes,
including nonzero codes, are successful operation results; start/read/wait failures use `ProcessError`.

Operational failures use `Result`, absence uses `Option`, and success-only results use `Result<void, E>`.
`Result::Ok` carries no payload when its success type is `void`. Generic void enum fields have no storage;
supplied void initializers still execute. Void callback parameters have no runtime argument, so
mapping a void success invokes a no-argument callback. Void unwrap operations return `void`.
Allocation failure and explicit failed unwrapping panic. File and standard-output writes handle
partial writes and interrupted operations. HTTP clients accept an explicit IPv4 connection address
separately from the Host header. Servers expose `serve_one()` and a looping `serve(on_start)`.
HTTP uses explicit connection addresses and connection-close responses with optional Content-Length
framing. Headers are limited to 64 KiB and bodies to 8 MiB. DNS, TLS, chunked transfer, and persistent
connections are unsupported. Prioritized language work is tracked in [TODO.md](TODO.md).

```zep
var path = String("output.bin")
var mut file = File::open(path, FileMode::Write).unwrap()
file.write(StringView::from_cstr("hello")).unwrap()
file.close().unwrap()

var mut arguments = FormatArguments()
arguments.add(42)
var text = format(String("value: {}"), arguments).unwrap()
print(text).unwrap()
```

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

A `return` statement has type `never`: that branch exits the function and does not determine the
value type of a surrounding `if` or `when` expression. A `when` subject and its patterns use their own
type context, independently of the arm results.

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
have compatible types. An `else` arm must be last. It can be omitted when unguarded patterns cover
every enum variant, or both `true` and `false` for a boolean subject. Guarded patterns do not establish
exhaustiveness. Missing enum variants are listed in the diagnostic. Patterns already covered by an
unguarded pattern, and redundant `else` arms after complete finite coverage, produce warnings.

V1 supports numeric, boolean, pointer-compatible subject matching, and enum variant matching with
named payload bindings. Enum patterns must refer to the subject's enum declaration; a specialized
enum alias must also match its generic arguments. String subject matching, ranges, deeper
destructuring, and numeric range exhaustiveness are deferred.

Payload bindings borrow the subject's fields and preserve its mutability. Reading into a new owner
copies; explicitly moving a binding operates on the original field. Bindings have no independent
destruction. Temporary subjects live through the full expression and are destroyed once.

### Loop Statements

```
while (condition) {
    body
}

for (var mut i: i32 = 0; i < 10; i = i + 1) {
    body
}

```

Loop conditions must be `boolean`. C-style `for` loops require initializer, condition, and step
clauses; the initializer is either a `var` declaration or an expression. Loops are statements with
`void` type. Array iteration and integer range syntax are not implemented.

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
```

- `#sizeof` evaluates at compile time and produces a constant `i64` value
- Applicable to all sized types except `void`
- `#length` evaluates at compile time for fixed-size arrays and produces a constant `i64` value

### Explicit Storage Lifetimes

`new (pointer) value` initializes raw storage without destroying its previous contents. It requires
an aligned, valid mutable pointer to uninitialized storage. The pointer is evaluated once, before
the value. The expression returns a mutable pointer to the initialized value.

Allocate storage explicitly through `Memory`, construct its value with placement `new`, invoke its
destructor, and release its storage with the matching allocator. There is no allocating-new or
delete expression. The compiler does not look up allocation functions implicitly.

`new` and `delete` are reserved keywords. The HTTP client request and server route helpers for
the DELETE method are named `remove`.

`value.~Type()` destroys a mutable place recursively, without freeing its storage. Destroying a named
local disables its scope-exit cleanup; assignment reconstructs it and enables cleanup again. The
caller tracks initialization when using raw pointers and must not destroy the same value twice.

The move helper is an ordinary generic static method in std:

```zep
import std.memory.Memory

var ptr: *mut String = Memory::allocate()
new (ptr) Memory::move(value)
ptr[0].~String()
Memory::free(ptr as *mut void)

new (storage) Memory::move(other)
storage->~Type()
```

`Memory::move` returns a move reference to its argument; it does not itself transfer or destroy a
resource. Construction and assignment select a move operation from that reference. The source
remains alive and destructible. Compiler checking and lowering use reference types and lifecycle
plans rather than recognizing the move helper's name.

### Inline Assembly

Named operands exchange integers and pointers with target assembly:

```zep
var mut total: i64 = 20
asm {
    "addq {rhs}, {sum}"

    in(rhs, "r", 22 as i64)
    inout(sum, "r", total)

    clobber("flags")
}
```

`asm` is a reserved keyword introducing a `void` block expression. One or more quoted instruction
templates appear first and are joined with newlines. Operand, clobber, and option declarations follow.
Semicolons between entries are optional. The former `#asm(...)` syntax is rejected with a migration diagnostic.

`in`, `out`, and `inout` declare named operands. Outputs require mutable destinations. `r` requests
a general register, `i` requests an immediate input, and register names such as `rax` request a
fixed register. Multiple outputs write to their destinations; the expression returns `void`.
Inputs and destination addresses are evaluated once. Placeholders use `{name}`; doubled braces
insert literal braces. Literal dollar signs are escaped for LLVM automatically.

Assembly is side-effecting and clobbers memory by default. `options("nomem")` promises no memory
access; `options("pure", "nomem")` additionally permits removal of unused computations and requires
an output. `options("att")` is the default x86 dialect; `options("intel")` selects Intel syntax.
List modified registers and flags with `clobber`. Named register operands initially target Linux
x86_64. Branches into language code, unwinding, and unbalanced stack manipulation are unsupported.

## Generics

```zep
fn identity<T>(value: T) -> T { return value }
struct Container<T> {
    public:
        var value: T
}
```

Generic constraints: `<T: Constraint>` — argument must be compatible with constraint type.

Integer and floating-point constraints accept their respective numeric families. Specialization
preserves the concrete argument's width and signedness throughout operators, conversions, and
nested generic calls; the constraint does not become the runtime type.

Typed arguments determine numeric generic bindings before default literal types. Numeric literals
can adopt the expected result's width within their numeric family. Instantiated receiver parameters
also provide context for arguments such as `Result::Ok { value: text }`.

Generic instantiation: `identity<i32>(42)`, `Container<i32> { value: 42 }`

Type arguments can be inferred from function arguments and literal fields:

```zep
var inferred = Container { value: 42 }
var contextual: Container<i64> = Container { value: 42 }
var value = identity(42)
```

The first container and `identity` call infer `i32`. The annotated container checks its field as
`i64`. Matching expected nominal types also contextualize nested struct/enum literals, return
expressions, and arguments of unambiguous non-generic calls. Explicit generic arguments take
precedence over expected-type inference.

Inference matches active generic parameters by declaration identity. It traverses pointers, arrays,
function types, and arguments of matching nominal declarations. Repeated occurrences of a parameter
must infer the same type; there is no common numeric type inference. Constraints are checked after
inference. Unresolved parameters require explicit generic arguments. Const-parameter inference,
partial `_` arguments, and expected-type-driven overload selection are not supported.

## Function Overloading

Multiple functions with the same name and different parameter types. Duplicate signatures produce an error.

## Ownership and Cleanup

### Value and Reference Semantics

Values are passed by value. Scalars and pointers copy directly. Aggregate lvalues use their copy
constructor; aggregate temporaries are constructed directly into the destination. A copy constructor
is declared as `fn Type(source: &Type) -> Type`. Default copying recursively copies aggregate
members, array elements, and the active enum payload. Raw pointers copy their address and never
destroy the pointee. Standard strings, vectors, and byte buffers define deep copies; boxes and
socket handles reject copies.

References borrow storage, cannot bind null, and do not destroy their referents. `&mut T` permits
mutation through a borrowed reference. A local reference bound to an owning temporary extends that
temporary's lifetime to the reference's scope. Obvious returns of references to local values are
diagnosed; references do not introduce a borrow checker.

Move constructors use `fn Type(source: move Type) -> Type`. Std's `move(value)` explicitly requests a
move. Moving leaves the source alive and destructible; resource implementations reset their source
to an empty state. Move-qualified methods use `fn method() move -> ReturnType` and are called on a
temporary or an explicit move, for example `move(result).unwrap()`.

Assignment special members use `fn operator=(source: &Type) mut -> &mut Type` and
`fn operator=(source: move Type) mut -> &mut Type`. `= default` requests generated member operations;
`= delete` makes an operation unavailable. A user-declared destructor or copy operation suppresses
implicit moves. Members that cannot support an operation make the corresponding generated
operation unavailable.

### Destructors

Structs declare a destructor as `fn ~Type() -> void`. Every owned aggregate containing a
destructible value receives implicit cleanup even when it has no declared destructor. Cleanup runs
at the end of a temporary's full expression, at scope exit, before `return`, and before overwriting
an initialized value. The user destructor body runs first, followed by fields in reverse declaration
order and then the base object. Arrays are destroyed in reverse element order and enum cleanup
destroys only the active payload. Module globals are destroyed in reverse initialization order.

Explicit destructor calls work on locals, members, indexed elements, and pointer pointees. Generic
code uses `value.~T()`; scalar types accept a matching destructor call as a no-op. Named locals
become uninitialized; reading or destroying them again is an error until assignment reconstructs
them. Raw-storage users remain responsible for tracking initialized members and elements.

Borrowed owning temporaries are retained through their full expression and destroyed in reverse
construction order. Consumed block results survive cleanup of the block's locals; discarded owning
results receive cleanup. Replacing an owner evaluates its replacement before destroying the old
value and captures member/index destinations once.

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
common/           Shared arenas, diagnostics, source paths, tagged nodes, scoped state, and hashing
frontend/         Compiler frontend
  ast/            AST node definitions and program root
  lexer/          Tokenizer
  parser/         Recursive descent parser with precedence climbing
  token/          Token types and keyword mappings
  sema/           Semantic analysis (type checker, scopes, resolvers, compile-time evaluator)
hir/              High-level IR representation
codegen/          Code generation orchestration and LLVM backend
workspace/        Manifest reader, project model, package graph, toolchain discovery
compiler/         Compilation pipeline, module graph, lowering, and monomorphization
builder/          Direct compiler, object generation, and linker orchestration
lsp/              Editor analysis, protocol transport, dispatch, sessions, codecs, and handlers
cli/              Command-line driver (build, compile, lsp, fetch, install)
```

### Compiler Pipeline

```text
Source → Lexer → Parser → AST → SemanticChecker → Lowerer → HIR → Codegen → LLVM IR
```

Lifecycle selection belongs to semantic analysis. `Lifecycle` caches immutable plans for copy,
move, assignment, and destruction, including selected functions, recursive aggregate parts, deleted
operations, and dependent operations. Body checking owns the destroyed-variable set and merges
branch and loop states through one operation. `Binding` checks reference binding and records
construction plans on coercions.

HIR lowering executes those plans through `Construction`. `Cleanup` owns scope actions and the
shared initialized-slot registration path for locals, parameters, and temporaries. `Temporaries`
captures temporary storage and preserves expression results before cleanup. Generic operations are
resolved through the same lifecycle service after specialization.

Semantic symbols live under `sema/symbols`, scope storage and environments under `sema/scopes`,
types and type resolution under `sema/types`,
constant values, arithmetic, and evaluation under `sema/constants`, and generic bindings and
substitutions under `sema/generics`. Symbol subclasses share tagged casting. Function signatures
come from the symbol's type; symbols do not store a second signature or choose linker names.
Constant evaluation distinguishes unresolved generic input from an invalid expression.

Facade metadata lives in `types.facades`, lookup and instantiation in `resolution.facades`, and
conversion policy in `types.compatibility`. Generic backing parameters use declaration identity and
an index into their definition. Member scopes belong to nominal types rather than type symbols.
Facade instances use the same cached instantiation path as named generic types and retain their
concrete generic arguments for backing-type resolution. Primitive receiver binding reuses that
instance when substituting parent generic parameters.

`CompileTimeValue` stores values and their stable representation. `ConstantMath` owns checked integer
construction, arithmetic, and casts, with one range policy and explicit literal/arithmetic diagnostics.
`CompileTimeValue` and `CompileTimeEnvironment` retain their names.

Compiler and codegen module namespaces and CMake target names are unchanged. Their C++ interfaces use
`Lowerer`, `Specializations`, `Backend`, `EmitOptions`, and `Target`, without aliases for the old names.

Build coordination lives in `zep.build`. Both project builds and single-file compilation use
`zep.build.objects` for HIR lowering and object emission. `zep.build.linker` owns linker command
construction and execution. Dependency fetching and installation live in `zep.build.fetch` and
`zep.build.install`; their consumers import those modules directly.

`zep.compiler.test_sources` discovers and loads test inputs before one semantic check.
`zep.compiler.tests` prepares test identities, private sibling access, selection, and dispatcher
source. `zep.build.tests` coordinates the test build and isolated processes. Project and test
builds share object generation: root module objects retain their module paths, dependency objects
live under a separate `dependencies/<package>/` directory, and test objects retain `.test` in their
filenames. Dependency modules cannot overwrite root-package objects with the same module path.

Lowering's `lifetime.cppm` groups construction, cleanup registration, full-expression temporaries,
and their shared `TemporaryNames` generator. Names are internal and do not change source-level
symbol identity. The main lowerer is the module interface, with call and type implementation units.

LLVM's `values.cppm` groups value scopes, places, casts, ABI conversions, and aggregate values.
Interface dispatch lives beside call emission. Assembly, control flow, declarations, types, target
setup, and function traversal retain their own modules.

Function symbols record a variant of `CallableKind`, `MethodKind`, and `ConstructorKind` rather
than one mixed enum or a separate extension flag. Explicit deletion
is immutable declaration metadata; lifecycle resolution owns inferred unavailability and invalidates
cached plans when availability changes. Linkage and ABI remain distinct decisions.

Completion uses typed receiver expressions and semantic visibility rules. It distinguishes static
and instance access, enum variants, type positions, and named arguments, excludes later locals,
and replaces complete identifiers. Lexer-based recovery handles unfinished calls and member access
without offering symbols inside comments or strings.

Call argument normalization and inference share the `resolution.arguments` module. Candidate
matching retains the winning argument order, generic arguments, signature, and substitution
environment. HIR call lowering has a focused `calls.cpp` implementation unit for direct calls, indirect
calls, captured closures, and specialization requests. It consumes recorded signatures instead of
repeating receiver substitutions. Specializations use declaration identity and semantic binding equality,
with structural type hashes and canonical generated names; lifecycle
calls use the same specialization cache. The LLVM `calls` module shares argument evaluation, ABI
conversion, call creation, and return handling across direct, indirect, and interface calls.

The `types.cpp` implementation unit finalizes concrete HIR types and registers their canonical names.
The public lowering module exports its coordinator without compatibility aliases. Missing semantic
call targets or construction plans are compiler errors rather than triggers for another resolution pass.

LLVM place emission owns address calculation and entry-block allocation. Value conversions own
scalar casts and C ABI representation changes. Calls, assembly, aggregate initialization, and control
flow receive their dependencies explicitly and reuse the HIR visitor for recursive expression emission.

Assembly has dedicated AST and HIR expressions. The semantic assembly module validates operands
and produces LLVM constraints and translated placeholders. The LLVM assembly module evaluates
operand expressions and destination addresses once, emits the validated instruction block, and
stores explicit outputs. Assembly does not use builtin or intrinsic call targets.

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
