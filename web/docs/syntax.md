# Lexical syntax

## Keywords

| Keyword | Purpose |
|---|---|
| `fn`, `static`, `override` | Functions and method modifiers |
| `test` | Named module-level test in a `.test.zep` file |
| `struct`, `interface`, `enum`, `type` | Type declarations and aliases |
| `import` | Import an exported symbol |
| `var`, `const`, `mut` | Bindings, constants, and mutability |
| `new` | Placement construction |
| `return` | Return from a function |
| `try`, `catch`, `throw` | Checked errors |
| `defer` | Scope-exit action |
| `do` | Block expression |
| `if`, `else`, `when` | Conditional and matching expressions |
| `while`, `for` | Loops |
| `extern` | Foreign declaration |
| `public`, `private` | Visibility |
| `true`, `false`, `null` | Boolean and null literals |
| `as`, `is` | Explicit conversion and type check |

`delete` is used in `= delete` special-member declarations; there is no delete expression.
`move` is a contextual reference qualifier, `throws` is an attribute name, and `error` is an ordinary identifier.

## Literals

| Literal | Default type | Context |
|---|---|---|
| `42` | `i32` | A numeric expected type contextualizes it |
| `3.14` | `f64` | A numeric expected type contextualizes it |
| `"hello"` | `cstr` | Zero-terminated literal storage |
| `'a'` | `char` | Character |
| `true`, `false` | `boolean` | Boolean |
| `null` | Null | [Compatibility rules](types.md#compatibility) |

String escapes include `\n`, `\r`, `\t`, `\\`, and `\"`.

## Operators

Binary precedence, lowest first:

| Group | Operators |
|---|---|
| Assignment | `=` |
| Error handling | `catch` |
| Logical or | `\|\|` |
| Logical and | `&&` |
| Equality | `==`, `!=` |
| Comparison | `<`, `>`, `<=`, `>=` |
| Type check | `is` |
| Conversion | `as` |
| Additive | `+`, `-` |
| Multiplicative | `*`, `/`, `%` |

Numeric operators preserve the selected numeric type. Comparisons and logical operators return
`boolean`. Plain enum equality compares variant tags; payload enums and aggregate values require
explicit field or pattern comparisons.

Unary `+` and `-` operate on numbers, `!` on booleans, and `*` dereferences a pointer.
`&value` produces an immutable pointer; `&mut value` requires a mutable place and produces a
mutable pointer. Reference types use the same `&` spelling in type positions; see
[memory](memory.md).

Postfix `.` accesses fields and methods, `->` first dereferences a pointer, and `::` selects static
members, nested types, or enum variants. Indexing uses `value[index]`.
