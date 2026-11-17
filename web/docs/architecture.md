# Compiler architecture

Compiler sources, CMake, and TODO live in `zep/`. Std, examples, and editor extensions remain at
the repository root. The VitePress site lives in `web/`, with pages in `web/docs/`.

| Component | Responsibility |
|---|---|
| `common` | Arenas, diagnostics, sources, shared nodes, processes |
| `frontend` | Lexer, parser, AST, semantics, dumpers |
| `workspace` | Manifests, packages, project/toolchain discovery |
| `compiler` | Module graph, checking, build driver, and test preparation |
| `lowerer` | Lowering, specialization, cleanup, and HIR definitions |
| `codegen/api` | Backend contract and options |
| `codegen/llvm` | LLVM types, emission, target setup |
| `lsp` | Editor analysis, sessions, protocol, handlers |
| `cli` | Build, compile, test, fetch, and LSP commands |
| `cmake` | Configuration, targets, installation, instrumentation |

```text
Source → Lexer → Parser → AST → SemanticChecker → Lowerer → HIR → Codegen → LLVM IR
```

Semantic checking owns type compatibility, binding, call selection, constants, and lifecycle plans.
Symbols retain declaration identities; scopes own lookup. Generic bindings and specialization keys
use declaration identity. Candidate matches retain inferred arguments and signatures rather than
repeating inference. Constants distinguish concrete, dependent, and invalid outcomes.

`Lowerer` consumes checked metadata without recreating semantic scopes or resolving calls again.
Construction executes lifecycle plans, cleanup orders scope actions, and temporaries preserve
results across cleanup. Generic operations use the same semantic services after specialization.
Missing checked decisions are diagnostics.

LLVM remains confined to the backend. Place evaluation and ABI conversion each have one owner.
Calls share argument/result handling across direct, indirect, interface, and closure dispatch.
Fallible functions return an internal status with hidden success/error storage pointers; nonthrowing
and C ABI calls keep their normal conventions. Error propagation follows explicit cleanup edges,
without platform unwinding.

Driver code lives in `compiler/src/driver/` and builds into `zep_compiler`. HIR definitions and
lowering live in `lowerer/` and share the backend-neutral `zep_lowerer` target. Their modules use
`zep.lowerer` and `zep.lowerer.nodes`, keeping LLVM out of analysis dependencies.
Package definitions and their graph share one module. HIR nodes and programs share one module.

The driver shares object generation across project, file, and test builds. Test discovery loads
one graph, checks once, compiles once, then runs each test in its own process. Component tests live
beside `src/`; mutable fixtures are copied into build-tree workspaces.

## Language server

Analysis owns snapshots, overlays, indexing, completion, and queries. Core owns sessions and dispatch;
protocol owns framing, codecs, and UTF-16 edits; handlers expose operations. It reuses the normal
compiler pipeline with overlays. See [editor support](editor.md) for supported features.
