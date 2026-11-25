#!/usr/bin/env bash
# Install the qtclient pack SDK (QtClientSdk: abi headers + QtClientMod.cmake + a config
# recording the ABI version, hostBuildId and Qt kit). Configures only — compiles nothing,
# needs no host build. Packs then build with find_package(QtClientSdk) alone.
#
#   scripts/lifecycle/build/sdk.sh            → build/qtclient-sdk/desktop
#   scripts/lifecycle/build/sdk.sh --wasm     → build/qtclient-sdk/wasm
#   QTCLIENT_SDK_DIR=<dir> overrides the install prefix (printed as QTCLIENT_SDK_DIR=…).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
QTCLIENT="$(cd "$HERE/../../.." && pwd)"
QT_PREFIX_SH="$QTCLIENT/tools/qt/qt-prefix.sh"
WASM=0
for arg in "$@"; do
  case "$arg" in
    --wasm) WASM=1 ;;
    -h|--help) sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "[sdk] unknown argument: $arg" >&2; exit 2 ;;
  esac
done
case "${COMP:-}" in browser|wasm) WASM=1 ;; esac

command -v cmake >/dev/null || { echo "[sdk] need cmake" >&2; exit 1; }
command -v ninja >/dev/null || { echo "[sdk] need ninja" >&2; exit 1; }

HOST_ID="${QTCLIENT_HOST_BUILD_ID:-${SOURCE_COMMIT:-$(git -C "$QTCLIENT" rev-parse HEAD 2>/dev/null || echo unknown)}}"
toolchain=()
if [[ "$WASM" -eq 1 ]]; then
  PLAT=wasm
  QT_KIT="${QT_WASM:-$(bash "$QT_PREFIX_SH" --wasm)}"
  QT_HOST="$(bash "$QT_PREFIX_SH")"
  [[ -f "$QT_KIT/lib/cmake/Qt6/qt.toolchain.cmake" ]] \
    || { echo "[sdk] Qt for WebAssembly missing — make qt WASM=1" >&2; exit 1; }
  EMSDK_DIR="$(bash "$QT_PREFIX_SH" --emsdk)"
  [[ -n "$EMSDK_DIR" ]] || { echo "[sdk] emsdk missing — make qt WASM=1" >&2; exit 1; }
  # shellcheck disable=SC1091
  source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
  toolchain=(-DCMAKE_TOOLCHAIN_FILE="$QT_KIT/lib/cmake/Qt6/qt.toolchain.cmake" -DQT_HOST_PATH="$QT_HOST")
else
  PLAT=desktop
  QT_KIT="${QT_PREFIX:-$(bash "$QT_PREFIX_SH")}"
  [[ -f "$QT_KIT/lib/cmake/Qt6/Qt6Config.cmake" ]] || { echo "[sdk] Qt missing — make qt" >&2; exit 1; }
fi

BUILD="$QTCLIENT/build/qtclient-sdk-build/$PLAT"
OUT="${QTCLIENT_SDK_DIR:-$QTCLIENT/build/qtclient-sdk/$PLAT}"
cmake -S "$QTCLIENT/sdk" -B "$BUILD" -G Ninja \
  ${toolchain[@]+"${toolchain[@]}"} \
  -DCMAKE_PREFIX_PATH="$QT_KIT" \
  -DQTCLIENT_HOST_BUILD_ID="$HOST_ID" >/dev/null
rm -rf "$OUT"
cmake --install "$BUILD" --prefix "$OUT" >/dev/null
test -f "$OUT/lib/cmake/QtClientSdk/QtClientSdkConfig.cmake" \
  || { echo "[sdk] install failed (no QtClientSdkConfig.cmake)" >&2; exit 1; }
echo "[sdk] $PLAT hostBuildId=$HOST_ID Qt=$QT_KIT → $OUT"
echo "QTCLIENT_SDK_DIR=$OUT"
