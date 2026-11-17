# Zep priorities

Track active cleanup progress and concrete remaining work here. Each implementation needs focused
regressions, documentation, and repository consumer migration when it changes public syntax or APIs.

## Active cleanup

### Repository structure and documentation

- [x] Move compiler components and build configuration into `zep/`; keep std at the repository root.
- [x] Separate compiler/repository paths and migrate installation, fixtures, examples, and tooling.
- [x] Replace the design document with focused MDX documentation and rewrite repository guides.
- [x] Add root Makefile targets for builds, tests, and compiler/std installation; remove scripts.
- [x] Remove CLI installation; use `~/.local/share/zep/{bin,libs}` through Make/CMake.
- [x] Use `nproc` with memory-bounded compilation and lightweight default debug symbols.
  Full debug symbols and ccache remain explicit opt-ins.
- [x] Validate a fresh build, the complete suite, installation, examples, extension, and path/link audits.

### HTTP API and notes example

- [x] Replace verb-specific server/client wrappers with enum-based `handle` methods and migrate consumers.
- [x] Share HTTP method spellings, preserve binary bodies, and simplify response/route APIs.
- [x] Add separate notes client/server projects with modular in-memory storage and CRUD commands.
- [x] Cover routing, method failures, binary bodies, storage ownership, and the client/server workflow.
- [x] Update usage documentation and run std, example, and repository validation.

### Compiler limits and everyday output

- [x] Lower plain enum comparisons through their tags and reject unsupported aggregate comparisons.
- [x] Initialize runtime globals in dependency/declaration order and destroy owners in reverse order.
- [x] Infer nonempty array literals while preserving contextual typing and ownership plans.
- [x] Make `print`/`eprint` best-effort `void` functions; keep checked writes and formatting explicit.
- [x] Migrate consumers and validate output failures, constant overflow, ownership, and HTTP examples.

### Remaining compiler identity cleanup

- [x] Route generic AST visits through expression/statement dispatch and preserve dump snapshots.
- [x] Qualify language callable names by module and retain hidden private helper definitions for generics.
- [x] Share nominal scope identity and index generic struct declarations by declaration identity.
- [x] Remove duplicate-symbol linker suppression and reject conflicting explicit linker names.
- [x] Validate the reproduced failures, complete suite, std/notes programs, ownership sanitizers, and Pong.

### LSP checked-error editing and navigation

- [x] Fix document-symbol ranges at their source and guarantee selection/child containment on the wire.
- [x] Centralize symbol naming, selection, and outline construction instead of hard-coded offsets.
- [x] Index catch bindings for hover, definition, references, highlights, and contextual completion.
- [x] Highlight `try`, `catch`, `throw`, and checked-error types without invalid token ranges.
- [x] Complete throwable types in annotations/handlers and show resolved error sets in signatures.
- [x] Preserve navigation and scope boundaries for nested protected expressions and handler arms.
  GDB confirmed that generic AST dispatch skipped checked-error expressions. Syntax indexing now
  uses the existing expression dispatcher; declaration highlighting shares token-backed selections.
- [x] Fix editor configuration handling and validate the extension grammar/build.
  The extension reads `zep.server.arguments`, restarts on configuration changes, and exposes a
  restart command. TypeScript compilation passes; the grammar already recognizes checked errors.
- [x] Run focused LSP regressions, the complete suite, protocol range invariants, and update documentation.

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

- A fresh `make` build succeeds under `zep/`; `make test` passes all 344 CTests, including 65 LSP
  cases, callback optimization sweeps, and removal of the CLI install command.
- All 16 MDX pages compile; documentation links/anchors resolve. The editor extension compiles.
- Make propagates build/test failures, rejects empty tests, and works from another directory.
- Fresh CMake defaults to `~/.local/share/zep`. Full, compiler-only, std-only, custom-prefix, and
  staged installs pass. An installed compiler builds/runs using its sibling `libs/` without relying
  on repository std. The existing home installation remains untouched.
- All 16 std tests pass at `-O0` and `-O2`, including best-effort printing and checked writes.
- Hello runs, all five notes tests pass, and Pong starts/shuts down normally under GDB.
  Live notes CRUD, stable IDs, validation, binary bodies, 404s, and 405s pass using temporary copies
  on port 18081, preserving an existing server on port 8080.
- The import audit covers 130 production modules/partitions without missing imports, cycles, or
  duplicate declarations. Tracked snapshots and old root build caches remain unchanged.

## Compiler correctness

- [x] Lower plain unbacked-enum comparisons through enum tags. Payload/aggregate comparisons receive
  semantic diagnostics. The GDB-reproduced LLVM comparison assertion has regression coverage.
- [x] Support nonconstant RAII global initialization with ordered startup and teardown. Cross-module
  constructors, locally handled errors, inferred owning arrays, and reverse destruction are covered.
- [x] Share generic AST dispatch with expression/statement dispatch. Checked-error/null dump
  regressions pass without rewriting snapshots; LSP indexing uses the same dispatcher.
- [x] Link private helpers referenced by imported generic specializations through module-qualified,
  hidden symbols. Separate consumers observe one helper definition and shared private state.
- [x] Qualify nominal generic arguments by declaration scope and index generic structs by identity.
  Same-named types and generic declarations from different modules remain distinct at runtime.

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
