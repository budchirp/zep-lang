# FFI and attributes

## Foreign declarations

```zep
extern fn putchar(value: i32) -> i32
extern fn printf(pattern: cstr, ...) -> i32
extern var native_counter: i32
```

Extern calls use the C ABI. Variadic calls promote booleans and integers narrower than `i32` to
`i32`, and `f32` to `f64`. C functions and C callbacks cannot propagate checked language errors.
Use wrappers to turn native failure codes into [throwable structs](errors.md).

## Attributes

| Attribute | Purpose |
|---|---|
| `@mangle`, `@mangle(true)`, `@mangle(false)` | Control language name mangling |
| `@name("symbol")` | Explicit linker spelling |
| `@os("linux", "macos")` | Include declarations for listed operating systems |
| `@arch("x86_64", "aarch64")` | Include declarations for listed architectures |
| `@section(".name")` | Place a global in a named section |
| `@align(16)` | Set power-of-two global alignment |
| `@throws(ErrorType)` | Declare checked errors |

Disabling mangling and custom names are invalid on generic or overloaded functions. OS and
architecture attributes accept one or more strings and include a declaration when any matches.
Section requires one string and alignment requires a valid numeric power of two; both apply to
global variables. Error annotations retain resolved types rather than string arguments.

Language symbols normally include module identity. Private helpers use hidden linkage so imported
generic bodies can call the original definition. Explicit names and C declarations retain their
requested spellings, even when equal to the source name. Duplicate explicit definitions are linker
errors; the linker does not silently permit multiple definitions.
