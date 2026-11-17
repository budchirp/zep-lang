# Notes

A small HTTP backend and a separate command-line client. The backend stores owned note strings in
memory. IDs increase monotonically and are not reused after deletion. Restarting the server clears
the database.

From the repository root, build both projects:

```sh
cmake-build-debug/cli/zep build --project examples/notes/server
cmake-build-debug/cli/zep build --project examples/notes/client
```

Start the server in one terminal:

```sh
examples/notes/server/build/notes_server
```

Use the client in another terminal:

```sh
examples/notes/client/build/notes create "Buy groceries"
examples/notes/client/build/notes list
examples/notes/client/build/notes read 1
examples/notes/client/build/notes update 1 "Buy groceries and coffee"
examples/notes/client/build/notes delete 1
```

Both use `127.0.0.1:8080`. Quote note bodies containing spaces. The client exits with `0` for success,
`1` for HTTP/transport failure, and `2` for invalid command usage. Output is best effort.

| Method | Path | Request body | Success |
|---|---|---|---|
| GET | `/notes` | Empty | `200`, tab-separated IDs and note text |
| POST | `/notes` | Note text | `201`, new ID |
| GET | `/notes/ID` | Empty | `200`, note text |
| PUT | `/notes/ID` | Replacement text | `200`, `OK` |
| DELETE | `/notes/ID` | Empty | `200`, `OK` |

Empty note bodies and invalid IDs return `400`; missing notes return `404`. Routes with an unsupported
method return `405`. HTTP framing supports content lengths and connection-close responses; chunked
encoding, persistent connections, TLS, authentication, and concurrent serving are outside this example.

The server keeps its `Api` owner alive longer than `HttpServer`. Routes retain borrowed `Handler`
interface values pointing to that owner. `Api` handles HTTP behavior; `Database` owns storage;
`Command` parses client arguments. There is no global database or erased context pointer.

Run the example tests:

```sh
cmake-build-debug/cli/zep test --project examples/notes/server
cmake-build-debug/cli/zep test --project examples/notes/client
```
