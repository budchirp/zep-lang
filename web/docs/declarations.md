# Declarations

## Variables and functions

```zep
var count = 3
var size: i64 = 8
var mut position = 0
const limit = 10

fn add(left: i32, right: i32) -> i32 {
    return left + right
}
```

Bindings are immutable unless marked `mut`; `const` requires compile-time evaluation.
Arguments can be positional or named: `add(1, right: 2)`. Positional arguments come first.
Overloads must have different parameter signatures. Const functions use `fn name() const -> T`.
Runtime globals initialize in dependency/declaration order and destroy in reverse order.

## Structs

```zep
struct Point {
    public:
        var x: i32
        var y: i32 = 0

        fn Point(x: i32, y: i32) -> Point {
            return Point { x: x, y: y }
        }

        fn sum() -> i32 {
            return x + y
        }
}
```

Structs contain fields, methods, and nested types. Literals supply required fields once; defaults
may be omitted. Constructors use the type name. Methods receive implicit `self`; `mut` permits
mutation, and `static fn` uses `Type::method()`. Nested types use `Outer::Inner`.
Module-level `fn Point.total() -> i32` adds an extension method.

A struct can inherit one base and list [interfaces](generics.md). Matching inherited/interface
methods require `override fn`. See [memory](memory.md) for special members and destructors.

## Enums

```zep
enum Shape {
    Point
    Circle { radius: f64 }
}

enum Packet(source: i64) {
    Empty
    Text { body: cstr }
}

var packet = Packet::Text { source: 7, body: "hello" }
```

Variants are newline-separated; payload fields are comma-separated and may have defaults.
Header fields are public and shared by all variants. Access uses `.source`; patterns may omit
shared fields. Duplicate names or missing required fields are errors.

Backed enums use `enum Key : i32 { W = 87 }`; `.value` exposes the backing value.
Discriminants must fit and be unique. Integer values may auto-increment; references to later
variants are invalid. Backed enums cannot contain runtime fields. Enums support methods but
not user constructors/destructors. Generic void payload fields have no runtime storage.

## Visibility and aliases

Declarations default to `private`; `public` makes them importable. Type members use `public:` and
`private:` sections. Outside code cannot access or initialize private fields.
`type Count = i64` creates a transparent alias; aliases may be generic.
