# Compiler correctness and small language improvements

## Current state

- Core implementation and final validation are complete.
- Final full debug suite: 259/259 passed (6.58 seconds), up from 229 at task start.
- Final regenerated diagnostic/cleanup probes are Valgrind-clean; applicable runtime tests pass at O0/O2. Pong builds and opens successfully; active snapshots are unchanged; `git diff --check` passes.
- Deferred work is explicitly listed below. No release builds or ownership redesigns were performed.
- Progress entries are chronological; earlier failures and pending notes are historical, not the current state.

## Approved scope

- Implement the core plan with simple, focused changes. Keep this file current after each batch, including failures, deferrals, and validation results.
- No release builds, automatic snapshot updates, unrelated refactors, commits, or index changes.
- Preserve existing ownership rules and evaluation order. Defer cleanup work requiring an ownership redesign.
- Deferred: `@must_use`, array iteration, `?`, slices, escaping closures, async, macros, partial `_` inference, and const-generic equation solving.

## 1. Diagnostic source lifetime

- [x] Reproduce diagnostics surviving package resolver destruction.
- [x] Preserve synthetic diagnostic source ownership through diagnostic copies/transfers without changing all borrowed compiler sources.
- [x] Test failed project opens, moved/copied diagnostics, and repeated errors; validate invalid reads with Valgrind.

## 2. Module globals

- [x] Reproduce cross-module mutable counter/accessor failure.
- [x] Fix classification/emission, preserving private global identity.
- [x] Test local/imported accessor calls, mutation, same-name private globals, and supported scalar/struct constant initializers at `-O 0` and `-O 2`.
- Example: `var mut count: i32 = 0`, with imported functions incrementing and reading it.
- Boundary: no runtime global initialization/destruction framework.

## 3. Bounded cleanup fixes

- [x] Reproduce missing destruction of discarded owning calls with a destructor trace.
- [x] Add bounded non-tail call-statement cleanup using existing drop generation. Preserve block tail values and local/place expressions.
- [x] Fix explicit destruction of imported owning types independently; imported generic/non-generic owners and FormatArguments pass.
- [x] Test destructor order, untaken/taken branches, call-result destination transfer, owning arguments, and borrowed place expressions. Final regenerated Valgrind probes pass for discarded calls and imported explicit destruction at O0/O2.
- Deferred: nested temporaries such as owning member-call receivers, full-expression cleanup, and consumed/discarded tail-value distinction. Existing cleanup records scope variables, not expression consumption; these need broader lowering context than this task allows.

## 4. Reliable generic inference

- [x] Share focused structural matching between calls and literals.
- [x] Bind only active generic parameters, using declaration identity.
- [x] Detect conflicting repeated bindings instead of overwriting; retain ordinary conversion checks separately.
- [x] Match nested nominal types by declaration identity and arguments, plus pointers, arrays, and functions.
- [x] Report unresolved/conflicting parameters; preserve constraints and overload isolation.
- [x] Test declaration identity across shadowed same-name parameters, unrelated same-name nominal definitions, constraints, repeated function types, nested inference, conflicts, and overload isolation.
- Boundary: no general solver or numeric common-type invention.

## 5. Contextual generic literals

- [x] Apply matching expected nominal type arguments before checking fields.
- [x] Support annotated locals, returns, and unambiguous monomorphic parameter types without changing overloaded selection rules.
- [x] Preserve explicit arguments and default inference without context.
- Example: `var box: Box<i64> = Box { value: 1 }` checks the literal as `i64`.
- [x] Test structs/enums, nesting, conflicting fields, unrelated expected types, monomorphic call parameter context, and two-optimization runtime behavior.
- Boundary: expected-type seeding currently handles type parameters only; const parameters and contextual overload solving remain deferred.

## 6. `when` correctness and boolean exhaustiveness

- [x] Validate enum declaration/specialization identity rather than names.
- [x] Diagnose missing variants and directly provable duplicate/unreachable enum/boolean patterns (warnings preserve existing accepted programs).
- [x] Recognize unguarded `true` plus `false` as exhaustive.
- [x] Preserve guarded-arm behavior and test terminating exhaustive branches at `-O 0` and `-O 2`.
- [x] Correct DESIGN.md: enum exhaustiveness, contextual/nested inference, and the remaining temporary-cleanup implementation limit.
- Example: `when (ready) { true -> 1, false -> 0 }` needs no `else`.

## Validation

- [x] Establish current debug baseline before production changes: 229/229 passed (4.37 seconds).
- [x] Add regression tests and run focused checks after each batch: 30 new CTests, 259 total.
- [x] Final debug build, full CTest, `git diff --check`, unchanged active snapshots.
- [x] Check module globals, inference/when, discarded owning calls, and imported explicit destruction at `-O 0` and `-O 2` using the debug compiler.
- [x] Use GDB to trace global lowering and cleanup specialization crashes; Valgrind verifies source and supported ownership lifetimes.

## Progress

- User selected the core plan, excluding optional features and ownership redesigns.
- Initial working tree is clean; previous refactoring work is committed in the current baseline. Replaced the completed REFACTOR_TODO.md with this checklist as requested.
- Historical result only: previous task reported 229 passing debug tests. Current baseline was independently confirmed above.
- Diagnostic lifetime reproduced under Valgrind before the fix: both new functional tests passed, but Valgrind exited 99 on invalid accesses (log `/tmp/opencode/zep_diagnostic_before.txt`). SourceLocation now optionally retains a synthetic source; copies of individual entries retain ownership. Ordinary compiler sources remain borrowed. Build/post-fix validation pending.
- Diagnostic batch: full debug build succeeded; 8 focused diagnostics/workspace tests passed. Post-fix Valgrind passes with zero errors (`/tmp/opencode/zep_diagnostic_after.txt`). Weak ownership test confirms release when retained entries are cleared.
- Globals regression reproduces missing accessor symbols before the fix. Module scopes have a parent, but `is_global()` only recognized parentless scopes, so module variables were incorrectly lowered as locals. Corrected scope classification; validation pending. An initial test edit landed inside a raw fixture string and failed C++ compilation; corrected before running the regression.
- After classification fix, 232/233 tests passed; new runtime regression still failed. GDB traced the remaining problem to inferred module globals: their predeclared symbols retained null types after initializer checking, causing dependent assignments to silently disappear. Retain symbol identity and fill its resolved type after checking the declaration; validation pending.
- Globals batch validated: full debug build and 233/233 CTests pass. Cross-module accessors, mutation, inferred aggregate globals, and separate private same-name globals run at both optimization levels. Direct public-variable import/linkage is not extended in this bounded batch.
- Cleanup regressions reproduced: discarded owning calls omitted destruction; explicit destructor calls were excluded from nominal member callable candidates (the old negative test passed for failed call resolution, not genuine use-after-drop checking). Added destructor candidates and bounded cleanup for discarded non-tail call statements only. Full nested-expression/branch-local temporary tracking and tail-value consumption require broader lowering context; defer those rather than change ownership architecture.
- Generic inference implementation underway: focused declaration-identity matcher replaces duplicate call/literal matching; preserves first binding, reports conflicts/unresolved parameters, and descends into matching nominal arguments. Named call arguments are normalized before final inference. Added regression suite including contextual cases; not yet validated.
- Cleanup and initial inference/contextual batches: full debug build and 245/245 CTests pass. Explicit destruction tests use mutable locals, as existing language rules require; the old use-after-destruction regression now asserts the exact genuine lifetime diagnostic.
- `when` improvements implemented; added focused semantic and two-optimization runtime tests, awaiting build. Global constant initialization now lowers evaluated values (including canonical struct field order), preventing valid constant arithmetic from becoming zero initializers; expanded global regression accordingly.
- Latest completed debug build/CTest: 257/257 pass (6.63 seconds). Includes five additional inference identity/constraint/function/context tests, `when` semantic tests, cross-module enum identity rejection, and the heap-owning cleanup trace at both optimization levels. Initial runtime `when` probe mixed i64 payload with default i32 zero; corrected the fixture with an explicit cast rather than extending unrelated arm coercion.
- Final globals hardening added: check global variable initializers before function bodies so inferred globals work even when referenced by earlier functions; emit supported char/string/string-array constants instead of zero. Expanded the runtime fixture and added rejection of runtime global initialization. These last edits and documentation await final build/validation.
- Follow-up validation: 258/259 tests passed; new globals fixture used a float literal, which the existing constant evaluator rejects. A proposed integer-to-float constant cast was also rejected; removed the float probe and unneeded float-emission extension instead of broadening constant evaluation. Char/string/string-array initialization remains covered. Float constant evaluation is deferred.
- O0 Valgrind probes passed: discarded-call fixture 7 allocations/7 frees, imported explicit-destruction fixture 3 allocations/3 frees, zero leaks/errors. At O2, the imported explicit-destruction probe exposed a missing generic field destructor symbol: another module's unused linkonce specialization was optimized away. Cleanup now requests the destructor specialization in its own output module using the existing per-output cache. Added O2 coverage to the explicit-destruction regression; validation pending.
- O2 specialization fix now links/runs and both O2 Valgrind probes report zero leaks/errors. GDB showed the hidden parent type was not imported into the consumer; bind its declaration-identity generic arguments and concrete parent directly. Full-suite testing then caught two formatting compiler crashes: nested destructor generation modified the active cleanup frame while it was being traversed. Reused CleanupFunctionGuard to isolate specialization generation; full validation pending.
- Cleanup-frame isolation verified: 259/259 debug tests pass (6.76 seconds), including formatting and both O0/O2 explicit destruction tests. Pong rebuild succeeds. Final source review made only local formatting/include cleanup; rebuilding and rerunning verification now.
- Final verification: debug build succeeds with no compiler warnings/errors in `/tmp/opencode/zep_final_core_build.txt`; 259/259 tests pass (6.58 seconds), output `/tmp/opencode/zep_final_core_tests.txt`. Snapshot paths have no diff; `git diff --check` passes.
- Final regenerated O0/O2 lifetime probes: discarded-call fixture has 7 allocations/7 frees; imported explicit-destruction fixture has 3 allocations/3 frees. All four runs have zero bytes in use at exit and zero errors. Logs: `/tmp/opencode/zep_cleanup_O{0,2}_valgrind.txt` and `/tmp/opencode/zep_explicit_O{0,2}_valgrind.txt`.
- Final diagnostic Valgrind run: 1,355 allocations/1,355 frees, zero bytes in use at exit and zero errors; `/tmp/opencode/zep_diagnostic_final.txt`.
- Final Pong build succeeds; three-second startup initializes Raylib/OpenGL/window successfully and exits via the expected timeout status 124. Log: `/tmp/opencode/zep_core_pong_startup.txt`.
- No release builds, commits, resets, or index operations performed. General nested/tail temporary cleanup, direct imported global-variable linkage, float constant evaluation, and the optional/larger features remain deferred as recorded above.
