#!/usr/bin/env bash
# Build the Qt network contract test with the pinned Qt wasm kit (tools/qt).
#   tests/browser/qt-network/build.sh [outDir]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../../.." && pwd)"
OUT="${1:-$HERE/out}"
QT_WASM="${QT_WASM:-$(bash "$ROOT/tools/qt/qt-prefix.sh" --wasm)}"
QT_DESKTOP="${QT_DESKTOP:-$(bash "$ROOT/tools/qt/qt-prefix.sh")}"
[[ -n "$QT_WASM" && -n "$QT_DESKTOP" ]] || { echo "[qt-network] Qt kits missing: make qt WASM=1" >&2; exit 1; }
if ! command -v emcc >/dev/null; then
  EMSDK_DIR="$(bash "$ROOT/tools/qt/qt-prefix.sh" --emsdk)"
  [[ -n "$EMSDK_DIR" ]] || { echo "[qt-network] emsdk missing: make qt WASM=1" >&2; exit 1; }
  # shellcheck disable=SC1091
  source "$EMSDK_DIR/emsdk_env.sh" >/dev/null
fi
BUILD="$OUT/build"
cmake -G Ninja -S "$HERE" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$QT_WASM/lib/cmake/Qt6/qt.toolchain.cmake" \
  -DQT_HOST_PATH="$QT_DESKTOP" >/dev/null
cmake --build "$BUILD"
for f in qtnetwork_contract.js qtnetwork_contract.wasm qtloader.js; do cp "$BUILD/$f" "$OUT/"; done
cp "$HERE/index.html" "$OUT/index.html"
echo "[qt-network] built $OUT"
