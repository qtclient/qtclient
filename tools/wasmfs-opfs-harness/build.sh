#!/usr/bin/env bash
# Build the PROXY_TO_PTHREAD + WasmFS OPFS smoke binary.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
OUT="${ROOT}/out"
mkdir -p "$OUT"

EMCC="${EMCC:-$(command -v emcc || true)}"
if [[ -z "$EMCC" ]]; then
  echo "emcc not on PATH; source emsdk_env.sh" >&2
  exit 1
fi

"$EMCC" "$ROOT/main.c" \
  -O2 \
  -sWASMFS \
  -pthread \
  -sPROXY_TO_PTHREAD \
  -sPTHREAD_POOL_SIZE=8 \
  -sPTHREAD_POOL_SIZE_STRICT=0 \
  -sEXIT_RUNTIME=1 \
  -sENVIRONMENT=web,worker \
  -sINITIAL_MEMORY=64MB \
  -sALLOW_MEMORY_GROWTH=0 \
  --post-js="$ROOT/post.js" \
  -o "$OUT/harness.html"

echo "Built $OUT — serve with:"
echo "  python3 apps/qtclient/tools/emscripten-web-serve.py --root $OUT --port 8765"
echo "  open http://127.0.0.1:8765/harness.html"
