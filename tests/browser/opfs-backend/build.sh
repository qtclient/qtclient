#!/usr/bin/env bash
# Build the /play file system contract test (no Qt): the WasmFS OPFS backend,
# the engine's Worker (browser/qtclient-fs.js) and the JSPI runtime, as the
# tibia / launcher engines link them. Needs emcc (the Qt kit's emsdk pin).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
OUT="${1:-$HERE/out}"
EMCC="${EMCC:-emcc}"
WASMFS_INCLUDE="$(dirname "$(command -v "$EMCC")")/system/lib/wasmfs"
mkdir -p "$OUT"
"$EMCC" -std=c++20 -O1 -fwasm-exceptions -pthread \
  -I"$ROOT/client/tibia" -I"$WASMFS_INCLUDE" \
  "$HERE/contract.cpp" "$ROOT/client/tibia/browser/qtclient_opfs_backend.cpp" \
  -sWASMFS -sJSPI -sPTHREAD_POOL_SIZE=2 -sINITIAL_MEMORY=64MB \
  -sENVIRONMENT=web,worker -sEXIT_RUNTIME=0 \
  --post-js "$ROOT/browser/jspi-runtime.js" \
  --post-js "$ROOT/browser/qtclient-fs.js" \
  -o "$OUT/contract.js"
cp "$HERE/index.html" "$OUT/index.html"
echo "[opfs-contract] built $OUT"
