# Repository refactoring checklist

## Current state

- A, B, C, and E are implemented and debug-validated. D was measured and deliberately deferred; see below.
- Latest full debug suite: 229/229 passed (5.66 seconds). Formatting ownership fixture: Valgrind 352 allocations/352 frees, zero leaks and zero errors.
- No active builds. User explicitly requested no further release builds on 2026-10-30. Earlier release build and 226 tests passed before the final generic-field cleanup fix and new tests; this is NOT final-release verification.
- Standard-library edits were made last, after A-C validation and measurement. Their new generic-field composition exposed a lowering issue; user separately approved that narrowly scoped fix.
- Entries in the progress log are historical. Do not resume old background jobs or treat old pending-state notes as current work.

## Scope and rules

- Treat the staged changes present at task start as intentional baseline work. Never reset, discard, unstage, or overwrite unrelated changes.
- User approved all audit refactors and internal C++ API breaks. Preserve language behavior, CLI, LSP, public standard-library APIs, diagnostics, snapshots, evaluation order, and ownership lifetimes.
- Keep code modular and straightforward. No code comments, unnecessary helper frameworks, or scripted source rewrites. Edit with patch tools. Follow AGENTS.md.
- Standard-library refactoring MUST be last, after all other refactors and validation.
- Keep this checklist current with findings, completed changes, remaining work, and test results so another context can resume.
- Do not repair the separately reported package diagnostic lifetime bug, specification mismatches, benchmark defects, or test-script defects without additional approval.

## Validation

- Baseline limitation: both presets configured, and all 154 core tests passed in both configurations. Full untouched CLI/LSP/Pong baseline was not established; see recorded build interruption. Do not report it as passing.
- [x] Build affected targets and run focused tests after meaningful groups; full-suite validation after A-C and standard-library work.
- [x] Final debug build and full CTest; final Pong build and bounded startup run.
- Final release verification waived by explicit user request. Earlier A-C release build and 226 tests passed.
- [x] Active snapshots unchanged; ZEP_UPDATE_SNAPSHOTS never set. GDB used to diagnose LLVM assertion; baseline source checks established unchanged call sites.
- [x] No claimed before/after speedups. Architectural optimization measured and gated as recorded below.

## A: Dead code and local simplification

- [x] Remove 51 unused AST and 29 unused HIR `accept()` member templates; active dispatch is visit_expression/visit_statement/visit_node. No callers/templates/callbacks require them.
- [x] frontend/src/parser/parser.cppm: remove unused private parse_enum_variant_expression; inline forwarding is_target_active and sole-use parse_stored_field; remove unused parent from parse_member_modifiers. Share for-loop condition/step/body parsing while preserving initializer branch.
- [x] frontend/src/ast/node.cppm: remove unused ResolvedCall and MemberModifiers::is_mutable_receiver.
- [x] codegen/llvm/src/type.cppm: remove unused private llvm_linkage (keep active DeclarationEmitter implementation).
- [x] compiler/src/lowering/lowerer.cppm and codegen/llvm/src/emitter.cppm: remove write-only current_function fields and associated save/restore/guards.
- [x] frontend/src/sema/scope/definition.cppm: remove unused DefinitionRegistry/FunctionDefinition, SemaContext field/import, declaration registration call. No runtime readers exist.
- [x] Remove unused MonomorphizationCache::clear_pending_specializations, FunctionEmitter::emit_function, ModulePath::source_path.
- [x] common/src/arena.cppm: own entries with vector<unique_ptr<T>>, preserve stable returned addresses/destruction order; default move construction, guarded self-move assignment; add destruction/move tests.
- [x] common/src/logger/logger.cppm: required Source reference instead of nullable private pointer; remove impossible null-source paths. Keep real null-location handling in Diagnostic::print.
- [x] frontend/src/sema/type_resolver.cppm: inline lookup_type_symbol; merge identical const/type parameter resolution, retaining parameter.kind.
- [x] frontend/src/sema/declaration/nominal.cppm: inline has_method_in_hierarchy comparison.
- [x] lowerer.cppm: inline is_lowering_monomorphized_body; directly return allocation from build_targeted_call_expression.
- [x] common/src/system/posix.cppm: read_pipe returns void, not ignored always-true boolean.
- [x] frontend/src/sema/type/type.cppm: remove unreachable later pointer switch implementation in Type::accepts; preserve earlier branch's behavior (including current void-pointer semantics).
- [x] codegen/llvm/src/scope.cppm: find once instead of contains + operator[]. Scope define_type/define_var/define_variant return emplace(...).second directly.
- [x] frontend/src/debug/type_dumper.cppm: hoist repeated indent/newline around dump switch. ast_dumper.cppm: inline dump_type forwarding and remove impossible nested empty-generics branch. External byte comparison passed.

## B: Structural consolidation

- [x] Remove OverloadSet wrapper and Env overload arena. Scope stores unordered_map<string, vector<FunctionSymbol*>>; callers/LSP updated. Added order/visibility/shadowing/reference-stability test.
- [x] Consolidate call/literal generic ordering into GenericArgumentResolver::apply; remove both reorder algorithms and forwarding binding helpers. Keep distinct reference/type-argument paths. Added reordered-argument and exact diagnostic-order/span tests; external comparison passed.
- [x] Merge enum checking into NominalDeclarationChecker; remove duplicate registration, dependencies, enum callback, and method-table construction. Retain real expression-checking callback.
- [x] ModuleGraph traversal takes already-loaded Module&, avoids duplicate loading, reuses cycle search; added diamond-graph identity/order test.
- [x] ModuleResolver removes always-false root flag and builds dependency-relative path once, retaining root lib-only resolution and local/file precedence.
- [x] HIRProgram owns nodes directly; HIRContext module and unused globals/functions vectors removed. Updated all 114 allocation sites and retained HIR identity/order tests.
- [x] AST uses shared operator classes through aliases; mapping/spelling moved to kind module; redundant casts removed. Intrinsic moved to kind; HIR no longer imports AST. Full build/tests pass.

## C: Tooling, backend, tests, build infrastructure

- [x] ModuleCatalog::complete is static and checks cursor/import context before local catalog discovery. No cache/invalidation state added.
- [x] Call scoring/inference use span suffix views; coercion checks authoritative parameter vector directly; always-false receiver option removed. Two passes retained.
- [x] LLVM leaf lowering precedes recursive validation; compound/pointer validation retained. Removed trivial naming/C-ABI helpers, temporary interface vector, and unused imports.
- [x] Removed FrontendChecks and updated callers/docs to existing assertion helpers.
- [x] Removed CodegenFormat module and unused format() metadata; retained backend injection and output-path assertions.
- [x] lsp/CMakeLists.txt uses zep_add_module.
- [x] cmake/Modules.cmake removes unused SOURCE_ROOT handling.
- [x] Removed only unreferenced root snapshot duplicate; active snapshots unchanged.
- [x] Update AGENTS.md/CONTRIBUTING.md architecture descriptions to current compiler/lowering, HIR, codegen/api and LLVM organization. README has no stale architecture listing. DESIGN semantics unchanged.

## D: Measurement-gated architectural optimization

- [x] Measure repeated generic registration with an external release-mode driver calling the actual registration method, twenty repetitions, one fresh lowerer per output module.
- Three-module fixture: mean analysis 219,684 ns, repeated registration 8,517 ns, full lowering 74,276 ns. Eight-module Pong: analysis 5,981,051 ns, registration 55,039 ns, full lowering 2,026,184 ns. Isolated registration probes exclude lowerer construction/destruction and precede the separately timed full lowering; these are characterization measurements, not before/after speedups.
- [x] Defer shared immutable registration architecture: roughly 55 microseconds on Pong (under 1% of analysis plus lowering in this sample) does not justify added ownership/state and name-fallback risk. Larger projects remain a future measurement opportunity. Specialization generation and pending definitions stay per output module.
- Reproducer: /tmp/opencode/zep_registration_benchmark.cpp and executable /tmp/opencode/zep_registration_benchmark. No production benchmark instrumentation added.

## E: Standard library LAST

- [x] FormatArguments uses Vector<FormatArgument>; removed duplicate allocation/growth/push/free implementation. Public overloads/render/size/is_empty/clear and destructor declaration retained. Empty public destructor preserves the interface; automatic field cleanup owns the Vector teardown.
- [x] Removed private REQUEST_BUFFER_SIZE, ErrorBody, and HttpServer::not_found. Networking/public APIs unchanged.
- [x] Characterization added and run before std edits: all scalar add overloads, borrowed strings, growth through 40 elements, bounds, clearing/reuse, and HTTP imports. Full debug suite passes after edits.
- [x] Formatting ownership fixture under Valgrind: 352 allocations, 352 frees, zero bytes in use at exit, zero errors. Named owned String locals isolate container ownership from unrelated temporary-expression cleanup behavior.

## Separately approved compatibility/correctness fixes

- [x] User approved replacing seven baseline getTerminator() null checks with hasTerminator(). LLVM 23 now asserts for unfinished blocks. GDB traced the abort; `git show :codegen/llvm/src/emitter.cppm` confirmed unchanged baseline call sites. Added conditional/loop/short-circuit codegen regression. Fourteen failing CLI tests then passed.
- [x] User approved generic-field cleanup correction required by FormatArguments composition. CleanupLowerer receives the authoritative lower_type operation via a focused callback, ensuring destructor names/types match emitted generic specializations. No generation cache is shared between modules. Added cross-module runtime test proving a generic field's destructor runs exactly once.
- Diagnostic evidence before the cleanup fix: cleanup requested Vector::~Vector$pS6VectorS14FormatArgument while nm showed emitted Vector::~Vector$pS24Vector$S14FormatArgumentS14FormatArgument. Formatting and synthetic cleanup tests now link/run.
- Untriaged findings left unchanged: explicit destruction of an imported FormatArguments value could not resolve before the storage edit; a global-counter version of the synthetic fixture failed to emit a global/accessor correctly; direct temporary String unwrap expressions leaked in the first ownership probe. The final fixture uses explicit owning locals rather than changing unrelated language/compiler behavior. These observations were not isolated on the untouched original tree, so do not claim proven pre-existing status.

## Deliberately retained boundaries and unrelated issues

- Keep CodegenBackend/ProcessRunner, AST/HIR phase boundary, meaningful lifetime/cleanup guards, public std convenience APIs and LSP callback boundaries.
- Do not merge resolver/lowering/LLVM concreteness predicates blindly: they check different properties.
- Keep TypeLayout checked dependency/overflow/cycle behavior distinct from byte_size.
- Separate issues discovered: PackageResolver-owned Source pointers may outlive resolver in diagnostics; DESIGN/current parser differ for imports/ranges/pointers/exhaustive when; benchmarks missing harness dependencies and compiler benchmark uses shared_ptr with dot; test scripts have stale optional-target paths and inconsistent example timeouts.

## Progress / results

- Initial audit: 133 production files, 32,807 lines, excluding generated artifacts and vendored Raylib. Existing staged changes are baseline. No source changes made during audit.
- Tool versions: CMake 4.4.4, Clang/LLVM 23.1.4; Ninja, LLDB and GDB available.
- Implementation started with this user-requested persistence checklist. Baseline results pending.
- Debug/release configuration succeeded. Initial builds both exceeded the tool's 120-second timeout without compiler errors; resumed both with 1,200-second limits. No production edits before baseline completion.
- Corrected architecture documentation while baseline binaries build. Added an external multi-module characterization fixture at /tmp/opencode/zep_refactor_characterization (named generics, literals, imported overloads, aliases, and loops); not yet run.
- Baseline subset: `ctest --test-dir cmake-build-debug --output-on-failure -E '^(Cli|Lsp)'`: all 154 discovered non-CLI/LSP tests passed (2.11 seconds). Full baseline pending CLI/LSP build completion.
- Arena implementation detail: retain the existing self-move-assignment guard when changing ownership; default vector move assignment does not promise the original self-move behavior. Add explicit lifetime/order/move coverage.
- Release baseline subset: the same 154 non-CLI/LSP tests passed (1.30 seconds).
- User reiterated immediate implementation. Applied most batch A after core baseline tests while original CLI/LSP builds still ran. This was a workflow mistake: deleting definition.cppm invalidated a source path referenced by existing baseline PCMs, interrupting debug LSP compilation. This failure is caused by our concurrent source deletion, NOT a pre-existing repository failure. Full untouched CLI/LSP/Pong baseline is therefore not established. Reconfigure/rebuild before further tests; do not claim full baseline success.
- Batch A edits and two new arena lifetime/move tests are applied but not yet compiled. `git diff --check` passes. Debug-dumper cleanup remains pending. Existing staged work preserved; std untouched.
- Debug reconfiguration succeeded after definition-module removal. Full refactored debug build is running with timeout disabled (shell sh_189195e14001SgRBqRVn4zj2cy). Do not edit production sources during the build or launch another build in that directory. Resume from its automatic completion notification; run CTest, fix our regressions, then continue remaining A and B. Original release baseline job sh_1890c49ce001r8fHZSDuUK2Iuq may also report the same missing-source failure; regenerate release only after it exits.
- Refactored common target compiled/linked; all 16 Common CTests passed, including both new arena tests (0.57 seconds).
- Original release baseline job exited on its 1,200-second timeout at task 141/175, without compiler errors. It is no longer running. Release must be configured/rebuilt against refactored sources later.
- Added external /tmp/opencode/zep_frontend_characterization.cpp and compiled it against the newly built frontend/common libraries. It captures every type-dumper kind plus null in all four indent/newline combinations and 12 named-generic call/literal cases (valid, reordered, duplicate, unknown, positional-after-named, wrong count). Saved output /tmp/opencode/zep_frontend_before.txt, SHA-256 92fc2f99b9906c144c3a4f5b39590cac55d96f7e18a346196efa80d27442ef24. Compare byte-for-byte after generic/dumper consolidation; do not rewrite this baseline.
- External characterization compile command (working directory cmake-build-debug): `clang++ -std=c++26 -g -O0 @frontend/tests/CMakeFiles/zep_frontend_tests.dir/unit/frontend_tests.cpp.o.modmap /tmp/opencode/zep_frontend_characterization.cpp frontend/libzep_frontend.a common/libzep_common.a -o /tmp/opencode/zep_frontend_characterization`; execute redirecting to a NEW after-output path and compare with cmp.
- All 156 core CTests passed after batch A (2.19 seconds). Found another externally launched build using the same debug directory; interrupted ONLY our own Ninja PID 220732 to stop concurrent artifact writes. Our build also reported a Clang ASTReader crash while duplicate compilers were writing the same modules; do not attribute it to source correctness without a single-build reproduction. External build PIDs 221964/221966 subsequently exited.
- Remaining A plus B/C are now applied. Deleted unused HIRContext/CodegenFormat modules only after both debug builds stopped. New tests cover overload stability/shadowing/visibility, reordered named generic arguments, exact diagnostic order/spans, and diamond imports. No standard-library edits yet. Regenerate debug and build before proceeding to measurements and std-last.
- A-C debug build completed. Initial full suite: 211/225 passed, fourteen CLI tests aborted on LLVM 23's changed terminator accessor contract. User approved compatibility fix; then 226/226 passed (5.88 seconds).
- A-C release configuration/build completed; 226/226 release tests passed (4.34 seconds). This predates the final cleanup callback fix and three later tests.
- External dumper/generic output after consolidation matched /tmp/opencode/zep_frontend_before.txt byte-for-byte using cmp.
- Pong built and initialized Raylib/OpenGL/window successfully; bounded `timeout 3s ./build/pong` exited 124 as expected. External named-generics/import/overload characterization project built and exited zero.
- Final full debug suite after standard-library and cleanup fixes: 229/229 passed (latest 4.70 seconds), output /tmp/opencode/zep_final_debug_tests.txt. `git diff --check` passes.
- Latest production-source diff versus preserved index: 45 files, 616 inserted lines and 1,780 removed lines (net -1,164), excluding tests/docs and excluding pre-existing staged work. Counts can change with final cleanup; do not combine with the user's staged patch.
- Final verification after the user's no-release-build instruction: debug reconfigure/build succeeded; 229/229 CTests passed (5.66 seconds), including ten newly added tests. Pong rebuilt and successfully initialized its graphical window during the final three-second bounded run. Active snapshot directories have no diff; git diff --check passes. No release build remains running.
- New tracked characterization fixtures are cli/tests/fixtures/format_arguments.zep and cli/tests/fixtures/http_modules.zep; tests copy their contents into disposable binary-tree workspaces before building. Existing staged changes remain staged and untouched by index operations.
