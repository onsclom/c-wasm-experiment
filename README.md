# c-wasm-experiment

Hand-written tokenizer and parser for a subset of C. The web frontend runs the parser as WebAssembly and visualizes the AST as an interactive force-directed graph.

## Development

Run the C test suite:

```bash
./build-and-run.sh
```

Run the web frontend:

```bash
cd web
bun install
bun run build-wasm      # rebuild after changes under src/
bun run dev
```
