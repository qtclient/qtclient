# tibia+gamecore WASM size (local Release, 2026-10-14)

| Artifact | Bytes | MiB |
|---|---:|---:|
| tibia.wasm | 48343222 | 46.1 |
| tibia.js | 317464 | 0.3 |
| tibia.html | 2985 | 0.0 |
| tibia.data | 6785560 | 6.5 |
| tibia.wasm.gz | 18718230 | 17.9 |
| tibia.js.gz | 71244 | 0.1 |

- Link: gamecore + Qt 6.10.3 wasm_multithread, INITIAL_MEMORY=512MB, SJLJ (WASM_USE_WASM_EXCEPTIONS=OFF).
- Serve: tools/emscripten-web-serve.py (COOP/COEP).
- Smoke: open tibia.html → account login → short play (Crystal WS :7174).
