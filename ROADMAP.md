# Zep feature roadmap

Estimates include implementation, meaningful regression tests, and documentation for one developer
familiar with the compiler. Dependencies overlap, so these estimates are not additive schedules.

| Priority | Feature | Complexity | Estimated effort |
|---|---|---|---|
| 1 | `break` and `continue` with scope cleanup | Low–medium | 2–4 days |
| 2 | Bitwise operations and shifts | Low–medium | 3–5 days |
| 3 | Compound assignments with one destination evaluation | Low–medium | 2–4 days |
| 4 | Borrowed slices with checked indexing | Medium–high | 1–2 weeks |
| 5 | `Result` propagation using `?` | Medium | 4–7 days |
| 6 | Range/array iteration and collection iterators | High | 2–4 weeks |
| 7 | Typed variadic formatting | High | 2–3 weeks |
| 8 | Hash maps and sets | Medium–high | 1–2 weeks |
| 9 | Compact enum payload storage | High | 2–3 weeks |
| 10 | Escaping captured closures | High | 3–5 weeks |
| 11 | Source-level debugger information | High | 2–4 weeks |
| 12 | Atomics and explicit memory ordering | High | 3–5 weeks |

Range syntax previously appeared in the design document, but the parser currently accepts only
C-style `for` loops. Range iteration belongs with the iterator work above.

DNS, TLS, chunked HTTP transfer, persistent HTTP connections, Windows, exceptions, a borrow checker,
and async remain separate projects. The current HTTP subset uses explicit connection addresses,
connection-close responses, and optional Content-Length framing. Headers are limited to 64 KiB and
declared bodies to 8 MiB; unsupported transfer encodings receive errors.
