# Named tests

```zep
import std.testing.Test

test "addition works" {
    Test::expect(2 + 3 == 5)
    Test::expect(4 > 0, "expected a positive value")
}
```

Tests belong in `.test.zep` files and have nonempty module-level names, unique within their module.
They behave as private zero-argument void functions with ordinary RAII. They cannot have modifiers,
attributes, parameters, value-bearing returns, or nested declarations of tests.

`src/math.test.zep` and `tests/math.test.zep` can access private declarations in `src/math.zep`.
Nested paths and index modules follow the same rule. Tests retain their own import scope.
Without a production sibling, a file is independent.

```sh
zep/cmake-build-debug/cli/zep test --project examples/notes/server
zep/cmake-build-debug/cli/zep test --project std --list
zep/cmake-build-debug/cli/zep test --input path/to/file.test.zep
```

Project discovery loads root-package sources and tests under `src/` and `tests/`. Dependency tests
and `tests/fixtures/` are excluded. Loaded tests are always checked; ordinary builds omit their code.
Filters match qualified names, ordering is deterministic, and an empty selection fails.

Tests compile once and run sequentially in separate processes with fresh globals. Filesystem state
is shared. Assertions use allocation-free panic, terminating that child without unwinding; remaining
tests still run. Normal completion performs RAII/global cleanup. Output, exit status, and signals
are reported. Hooks, skips, timeouts, and parallel execution are not implemented.

Use `make test` for repository C++/runtime coverage and `TEST_ARGS` to filter CTest. Std tests live
at `std/tests/`. See [contributing](https://github.com/budchirp/zep-lang/blob/main/CONTRIBUTING.md) for snapshots and optional tooling.
