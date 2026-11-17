# Checked errors

Errors are concrete structs implementing `std.core.Throwable`, directly or through `Error` or
`Exception`. Enums may describe reasons inside an error but cannot be thrown themselves.

```zep
import std.core.Error

struct ReadError : Error {
    public:
        var code: i32
}

@throws(ReadError)
fn read() -> i32 {
    throw ReadError { code: 5 }
}

fn fallback() -> i32 {
    return read() catch {
        error: ReadError -> error.code,
    }
}
```

`@throws(E1, E2)` declares escaping errors. Every fallible expression still needs `try`, `catch`,
or an enclosing protected `try` block, including discarded calls.

```zep
var value = try {
    var first = read()
    first + read()
} catch {
    error: ReadError -> 0,
}
```

`try expression` propagates to an outer handler or declared boundary. Catch binds an owned error;
arm values must match the success type. Partial catches need outer handling or explicit propagation.
Errors raised in handlers are handled outside that catch. Duplicate/impossible handlers are invalid.

Callbacks use `@throws(ReadError) () -> i32`. Error sets are order-independent and must match for
callback assignment/interface implementation. Lambdas infer errors or use their expected signature.

Constructors may throw; copy/move special members, destructors, main, tests, and C ABI boundaries
cannot propagate checked errors. Propagation transfers ownership and runs RAII cleanup without
platform unwinding. Panic, allocation failure, and signals remain separate.
