# Zep priorities

Track active cleanup progress and concrete remaining work here. Each implementation needs focused
regressions, documentation, and repository consumer migration when it changes public syntax or APIs.

## Active cleanup

Update this checklist whenever a cleanup stage is implemented or validated.

- [x] Remove inline assembly completely: syntax, tokens, AST/HIR metadata, semantic checking,
  lowering, LLVM emission, diagnostics, completion, tests, examples, and documentation.
  Removed the unused `in` keyword as well; the supported loop syntax remains C-style `for`.
- [x] Review the working diff and simplify the new error-handling implementation.
- [x] Move error decisions out of the context definition into their owning subsystem.
- [x] Keep contexts focused on shared compilation state and explicit dependencies.
- [x] Merge small classes into cohesive owners; keep modules and classes organized by responsibility.
  Error routing now belongs to `Errors`, handler cleanup belongs to `Temporaries`, facade entries
  belong to `FacadeRegistry`, and lifecycle parts belong to `LifecyclePlan`.
- [x] Clean up semantic type modules and lifecycle organization without duplicating policies.
  Type representation partitions now live under `types/`; error metadata/checking live under `errors/`.
- [x] Use plural subsystem names and singular entity names; preserve public compiler/codegen namespaces.
  Semantic collection directories now use `declarations`, `expressions`, `lifecycles`, `types`,
  `symbols`, `scopes`, `generics`, `constants`, and `errors`; phase names such as `checking` stay verbs.
- [x] Expand cramped code vertically and separate logical steps with blank lines throughout touched code.
- [x] Remove unused imports, dead code, obsolete wrappers, and duplicate algorithms in the changed
  subsystems after checking consumers.
- [x] Replace compiler-core C++ throws with explicit failure results and source diagnostics; use the
  shared diagnostic rendering/logger path for CLI and LSP instead of exception-based control flow.
  Parser recovery, lowering, specializations, backend calls, and target setup now use diagnostic
  state or explicit optional/expected results. Malformed-input and pipeline-failure regressions pass.
  Remaining workspace configuration and LSP protocol exception paths are listed separately below.
  Source loading now reports canonicalization failure through its existing optional result;
  the throwing path wrapper has been removed.
  Parser synchronization and failure-state reset now share one boundary-aware recovery operation.
  GDB identified a shared function-body caller that dereferenced failed lowering results; it now
  propagates failure. Regular and specialized functions share defaulted/explicit body handling.
  Regression checks cover missing call metadata, unsized specialized queries, invalid targets,
  dependent naming failures, and malformed parser input without compiler exceptions.
- [x] Update repository consumers, module documentation, and this checklist alongside implementation.
- [x] Run configure/build, affected tests, the complete suite, std tests, generated-code sanitizers,
  callback optimization checks, Pong under GDB, and focused dependency/dead-code audits.

## Latest validation

- Debug configure/build succeeds with normal optional-tool defaults restored.
- All 326 CTests pass, including callback optimization sweeps and compiler failure regressions.
- All 14 std tests pass at `-O2`; checked-error runtime fixtures pass at `-O0` and `-O2`.
- All nine generated checked-error/ownership tests pass under ASan/LSan.
- Lexer/parser fuzz smoke targets and 300 seeded runs per fuzzer pass.
- Frontend, lowering, and LLVM benchmark executables complete.
- Pong builds and exits normally under GDB; malformed parser input also completes under GDB.
- The read-only import audit covers 131 modules/partitions with no cycles or duplicate declarations.
- An exact clone scan across 52 sema/lowering/LLVM files finds no repeated nine-line windows.
- Tracked snapshots remain unchanged; obsolete assembly consumers and compiler-core C++ throws are absent.

## Compiler correctness

- [ ] Emit private non-generic helper dependencies referenced by cross-module generic
  specializations. Cover separate consumers and ensure each dependency has one definition.
- [ ] Qualify nominal generic arguments in generated symbol names. Distinct same-named types
  from different modules must not share a specialization's linker spelling; semantic cache identity
  alone does not prevent symbol collisions.

## Legacy error boundaries

- [ ] Replace workspace manifest/module-path validation throws with explicit failures and diagnostics.
- [ ] Replace LSP protocol/session validation throws with structured JSON-RPC failure results.
  Keep malformed JSON handling at the library boundary and preserve request error codes.

## Everyday language features

| Priority | Feature | Scope | Complexity |
|---|---|---|---|
| 1 | Loop control | Add `break` and `continue`, running cleanup for exited scopes. | Low–medium |
| 2 | Bitwise operations | Add integer bitwise operators and shifts with defined width rules. | Low–medium |
| 3 | Iteration | Add an iterator protocol and collection loops after slices and loop control. | High |

## Runtime and debugging

- [ ] Store only the active enum payload, preserving shared fields, layout checks, and cleanup order.
- [ ] Emit source-level debugger information for functions, locals, and generated specializations.
