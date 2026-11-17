# Modules and imports

Project production sources live under `src/`. Imports name one public symbol:

```zep
import geometry.Point
import geometry.Point as Position
import std.io.print
```

`import a.symbol` loads `src/a.zep` or `src/a/index.zep`; a file takes precedence over its index
alternative. `import a.b.symbol` uses `src/a/b.zep` or `src/a/b/index.zep`. Dependency prefixes
resolve through the [manifest](projects.md). For example, `std.io.print` resolves std's
`src/io/index.zep`.

Imported public symbols retain their visibility, so index modules can re-export canonical names.
Whole-module aliases, brace-list imports, and `public import` are not implemented.

The compiler tracks canonical physical sources to avoid loading the same file through different
paths. Source-level names and display names do not determine generated callable identities: module
identity qualifies language symbols, and nominal encodings include declaration scope. Explicit
linker names are covered in [FFI and attributes](ffi.md).

Named tests have their own discovery and private sibling rules; see [testing](testing.md).
