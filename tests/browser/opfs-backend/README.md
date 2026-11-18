# /play file system contract test

Runs the engine's file system — the WasmFS OPFS backend
(`client/tibia/browser/qtclient_opfs_backend.cpp`) and its Worker
(`browser/qtclient-fs.js`) — in headless Chromium and WebKit (Safari's engine) with real OPFS and JSPI, using
plain POSIX calls as Qt's file APIs make them: offsets and sparse writes,
truncate, the QSaveFile temp-and-rename pattern, directory listing, removal and
renames (directories included), the access-handle cap (`EMFILE`, never a wait)
and file I/O from a pthread, including while main blocks in `join()` (the
main-thread futex-wait wrapper, `client/tibia/browser/main_thread_wait.cpp`,
keeps that from hanging WebKit).

It guards what we do not own: Emscripten's WasmFS backend interface and the
browser's OPFS / Worker behaviour. CI: `CI: checks` → `/play file system contract`.

```bash
bash tests/browser/opfs-backend/build.sh            # needs emcc (tools/qt/qt-version.sh pin)
cd tests/browser/opfs-backend
npm install --no-save --no-package-lock playwright@1.64.1 && npx playwright install chromium webkit
node run.mjs out            # Chromium
node run.mjs out webkit     # WebKit
```
