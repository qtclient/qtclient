#!/usr/bin/env bash
# Print the CMake prefix of the CIP Qt kit for this host (--wasm / --emsdk /
# --android / --ios), or nothing when it is not installed.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=qt-version.sh
source "$HERE/qt-version.sh"
QT_ROOT="${QT_ROOT:-$HOME/Qt}"

SHA="$(qt_cip_sha)"
[[ -n "$SHA" ]] || exit 0
SHORT="${SHA:0:12}"

_print_if_qt() {
  local PREFIX="$1"
  if [[ -f "$PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    if command -v cygpath >/dev/null; then cygpath -m "$PREFIX"; else echo "$PREFIX"; fi
    return 0
  fi
  return 1
}

case "${1:-}" in
  --emsdk)
    [[ -x "$QT_ROOT/emsdk/emsdk" ]] && echo "$QT_ROOT/emsdk"
    exit 0
    ;;
  --wasm)
    _print_if_qt "$QT_ROOT/cip-$SHORT/wasm_multithread" || true
    exit 0
    ;;
  --android)
    for DIR in android_arm64_v8a android_armv7 android; do
      if _print_if_qt "$QT_ROOT/cip-$SHORT/$DIR"; then exit 0; fi
    done
    exit 0
    ;;
  --ios)
    _print_if_qt "$QT_ROOT/cip-$SHORT/ios" || true
    exit 0
    ;;
  *)
    HOST_DIR="$(qt_host_dir_name)"
    _print_if_qt "$QT_ROOT/cip-$SHORT/$HOST_DIR" || true
    exit 0
    ;;
esac
