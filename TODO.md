# Zep priorities

Only unfinished work with a concrete benefit belongs here. Each item needs focused regressions,
documentation, and repository consumer migration when it changes public syntax or APIs.

## Compiler correctness

- [ ] Emit private non-generic helper dependencies referenced by cross-module generic
  specializations. Cover separate consumers and ensure each dependency has one definition.

## Everyday language features

| Priority | Feature | Scope | Complexity |
|---|---|---|---|
| 1 | Loop control | Add `break` and `continue`, running cleanup for exited scopes. | Low–medium |
| 2 | Bitwise operations | Add integer bitwise operators and shifts with defined width rules. | Low–medium |
| 3 | Compound assignment | Evaluate the destination once and reuse assignment/lifecycle policy. | Low–medium |
| 4 | Result propagation | Add `?`, preserving early-return cleanup and error types. | Medium |
| 5 | Borrowed slices | Share array/container access through length-aware views and checked indexing. | Medium–high |
| 6 | Iteration | Add an iterator protocol and collection loops after slices and loop control. | High |

## Standard library

- [ ] Add hash maps and sets after hashing/equality contracts are defined; retain one storage policy.
- [ ] Add typed variadic formatting to replace manual argument containers while retaining owned text.

## Runtime and debugging

- [ ] Store only the active enum payload, preserving shared fields, layout checks, and cleanup order.
- [ ] Emit source-level debugger information for functions, locals, and generated specializations.

## Acceptance

- Keep compiler semantics authoritative; std and lowering consume resolved decisions.
- Add the smallest regression that exposes each defect; avoid duplicated smoke tests.
- Use representative unoptimized/optimized runtime cases. Reserve full `-O0`–`-O3` sweeps for
  assembly, ABI, and optimization-sensitive regressions.
- Run normal configure/build and CTest. Use ASan/LSan and GDB for ownership/runtime changes.
- Preserve tracked snapshots unless an intentional change is explicitly approved.
