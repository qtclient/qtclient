# WasmFS OPFS harness

Smoke binary for stock Emscripten WasmFS OPFS: `-sWASMFS -pthread
-sPROXY_TO_PTHREAD` with `wasmfs_create_opfs_backend()` at `/home/web_user`.

**Product `/play` tip does not use this path.** tibia.wasm keeps main-thread Qt
(no PROXY / no JSPI); durable tip is portal `Module.tipFs` (OPFS Worker + SAB)
plus the Qt `QAbstractFileEngine` in `client/tibia/browser/opfs_file_engine.cpp`.

## Build / serve

```bash
source "$(path-to)/emsdk_env.sh"
./build.sh
python3 ../emscripten-web-serve.py --root out --port 8765
# open http://127.0.0.1:8765/harness.html
```

Use a large enough pthread pool (`PTHREAD_POOL_SIZE≥8`, `STRICT=0`): OPFS
spawns a dedicated ProxyWorker on top of the proxied main thread.

## OPFS layout (portal writers must match)

WasmFS OPFS backend root is `navigator.storage.getDirectory()` (origin OPFS).

| VFS path | OPFS path |
|----------|-----------|
| `/home/web_user/<rel>` | `<rel>` |

Example tip file:

- VFS: `/home/web_user/.local/share/IdleOT/IdleOT/packages/IdleOT.app/…`
- OPFS: `.local/share/IdleOT/IdleOT/packages/IdleOT.app/…`

There is **no** `idleot-cip-home/` prefix — writers must use the table above.
