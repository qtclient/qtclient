#!/usr/bin/env bash
# Gate: a pack builds against the installed pack SDK alone (host and pack decoupled).
#
#   1. install the SDK (sdk/, QtClientSdk) to a prefix outside this tree — or take
#      an installed one (--sdk DIR)
#   2. static contract: the pack's CMake has no host path / host target
#      (add_subdirectory of qtclient, TIBIA_ROOT, QTCLIENT_ROOT, target_*(tibia …))
#   3. configure the pack with CMAKE_PREFIX_PATH=<sdk> only (the SDK config hints its
#      recorded Qt kit); fail if the pack build graph names this qtclient tree
#   4. build + install the pack, then check the installed tree:
#        desktop: mods/<id>/mod.json + native/mod.* with zero tip imports
#        wasm:    lib/cmake/QtClientPack_<c_id>/ + lib/libqtclient_mod_<id>.a
#                 (zero tip imports, defines qtclient_mod_<c_id>_register_abi), no native/
#
# Usage: check-pack-sdk-build.sh --pack DIR [--wasm] [--sdk DIR] [--prefix DIR]
#                                [--build-dir DIR] [--configure-only] [--keep]
#   --prefix DIR      install the pack there and keep it (release uses it as THE pack
#                     build; the tip then takes -DQTCLIENT_PACKS=DIR)
#   PACK_SDK_BUILD=configure   same as --configure-only
# Needs the pinned Qt (make qt; WASM=1 for --wasm). Skips (exit 0) when the kit or
# pack is absent and CI is unset; fails closed in CI.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
QTCLIENT="$(cd "$HERE/../.." && pwd)"
PACK=""
WASM=0
CONFIGURE_ONLY=0
KEEP=0
SDK=""
PREFIX=""
BUILD=""
[[ "${PACK_SDK_BUILD:-}" == "configure" ]] && CONFIGURE_ONLY=1
while [[ $# -gt 0 ]]; do
  case "$1" in
    --pack) PACK=$2; shift 2 ;;
    --wasm) WASM=1; shift ;;
    --sdk) SDK=$2; shift 2 ;;
    --prefix) PREFIX=$2; shift 2 ;;
    --build-dir) BUILD=$2; shift 2 ;;
    --configure-only) CONFIGURE_ONLY=1; shift ;;
    --keep) KEEP=1; shift ;;
    *) echo "usage: $0 --pack DIR [--wasm] [--sdk DIR] [--prefix DIR] [--build-dir DIR] [--configure-only] [--keep]" >&2
       exit 2 ;;
  esac
done

say() { echo "[pack-sdk-build] $*"; }
die() { echo "[pack-sdk-build] FAIL: $*" >&2; exit 1; }
skip() {
  if [[ -n "${CI:-}" ]]; then die "$*"; fi
  say "skip: $*"
  exit 0
}

[[ -n "$PACK" ]] || { echo "usage: $0 --pack DIR …" >&2; exit 2; }
[[ -f "$PACK/CMakeLists.txt" ]] || skip "no pack CMakeLists.txt at $PACK"
PACK="$(cd "$PACK" && pwd)"
command -v cmake >/dev/null || skip "cmake not on PATH"
command -v ninja >/dev/null || skip "ninja not on PATH"

# --- 2. static contract (comments stripped: files may describe the host flow) ---
while IFS= read -r f; do
  if sed -e 's/#.*$//' "$f" \
    | grep -nE 'add_subdirectory\([^)]*qtclient|TIBIA_ROOT|QTCLIENT_ROOT|target_[a-z_]+\([[:space:]]*tibia([[:space:]]|\))|TARGET[[:space:]]+tibia([[:space:]]|\))|qtclient/(client|cmake|include)/'; then
    die "${f#"$PACK"/} reaches into the host tree / tip targets (build with find_package(QtClientSdk) only)"
  fi
done < <(find "$PACK" -name CMakeLists.txt -not -path '*/build/*' -not -path '*/third_party/*')
grep -q 'find_package(QtClientSdk' "$PACK/CMakeLists.txt" \
  || die "pack CMakeLists.txt must find_package(QtClientSdk)"

# --- Qt kit / toolchain ---
QT_PREFIX_SH="$QTCLIENT/tools/qt/qt-prefix.sh"
toolchain=()
if [[ "$WASM" -eq 1 ]]; then
  PLAT=wasm
  QT_KIT="$(bash "$QT_PREFIX_SH" --wasm 2>/dev/null || true)"
  QT_HOST="$(bash "$QT_PREFIX_SH" 2>/dev/null || true)"
  EMSDK_DIR="$(bash "$QT_PREFIX_SH" --emsdk 2>/dev/null || true)"
  [[ -n "$QT_KIT" && -f "$QT_KIT/lib/cmake/Qt6/qt.toolchain.cmake" && -n "$EMSDK_DIR" ]] \
    || skip "wasm Qt kit / emsdk missing (make qt WASM=1)"
  # shellcheck disable=SC1091
  source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
  toolchain=(-DCMAKE_TOOLCHAIN_FILE="$QT_KIT/lib/cmake/Qt6/qt.toolchain.cmake" -DQT_HOST_PATH="$QT_HOST")
else
  PLAT=desktop
  QT_KIT="${QT_PREFIX:-$(bash "$QT_PREFIX_SH" 2>/dev/null || true)}"
  [[ -n "$QT_KIT" && -f "$QT_KIT/lib/cmake/Qt6/Qt6Config.cmake" ]] || skip "desktop Qt kit missing (make qt)"
fi

WORK="$(mktemp -d "${TMPDIR:-/tmp}/pack-sdk-build.XXXXXX")"
if [[ "$KEEP" -eq 0 ]]; then
  trap 'rm -rf "$WORK"' EXIT
fi

# --- 1. SDK ---
if [[ -z "$SDK" ]]; then
  SDK="$WORK/qtclient-sdk"
  say "$PLAT: install SDK → $SDK"
  cmake -S "$QTCLIENT/sdk" -B "$WORK/sdk-build" -G Ninja \
    ${toolchain[@]+"${toolchain[@]}"} -DCMAKE_PREFIX_PATH="$QT_KIT" >/dev/null
  cmake --install "$WORK/sdk-build" --prefix "$SDK" >/dev/null
fi
SDK="$(cd "$SDK" && pwd)"
case "$SDK/" in
  "$QTCLIENT/sdk/"*|"$QTCLIENT/include/"*|"$QTCLIENT/cmake/"*) die "--sdk must be an installed SDK, not the qtclient source tree" ;;
esac
test -f "$SDK/lib/cmake/QtClientSdk/QtClientSdkConfig.cmake" || die "no QtClientSdkConfig.cmake under $SDK"
extra_headers="$(find "$SDK/include/qtclient" -name '*.h' ! -path '*/qtclient/abi/*' | head -5)"
[[ -z "$extra_headers" ]] || die "SDK ships non-abi headers: $extra_headers"

# --- 3. configure against the SDK alone ---
BUILD="${BUILD:-$WORK/pack-build}"
INSTALL="${PREFIX:-$WORK/pack-install}"
say "$PLAT: configure $PACK against $SDK"
cmake -S "$PACK" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}" \
  ${toolchain[@]+"${toolchain[@]}"} \
  -DCMAKE_PREFIX_PATH="$SDK" \
  -DCMAKE_INSTALL_PREFIX="$INSTALL" >/dev/null
# Nothing in the pack's build graph may point into this qtclient checkout's sources
# (an SDK installed under its build/ is fine).
if grep -n "$QTCLIENT/" "$BUILD/build.ninja" | grep -v "$QTCLIENT/build/" | head -5 | grep .; then
  die "pack build graph references $QTCLIENT (host source path)"
fi
say "$PLAT: configure ok (no host paths in build.ninja)"
if [[ "$CONFIGURE_ONLY" -eq 1 ]]; then
  say "ok (configure only)"
  exit 0
fi

# --- 4. build + install + check ---
cmake --build "$BUILD"
rm -rf "$INSTALL"
cmake --install "$BUILD" >/dev/null
mod_json="$(find "$INSTALL/mods" -mindepth 2 -maxdepth 2 -name mod.json 2>/dev/null | head -1)"
[[ -n "$mod_json" ]] || die "installed pack has no mods/<id>/mod.json"
mod_dir="$(dirname "$mod_json")"
id="$(basename "$mod_dir")"
c_id="$(printf '%s' "$id" | tr -c 'A-Za-z0-9_' '_')"
if [[ "$WASM" -eq 1 ]]; then
  [[ ! -d "$mod_dir/native" ]] || die "wasm pack must not install native/ (static-ABI)"
  test -f "$INSTALL/lib/cmake/QtClientPack_${c_id}/QtClientPack_${c_id}Config.cmake" \
    || die "missing static-ABI export lib/cmake/QtClientPack_${c_id}/"
  lib="$INSTALL/lib/libqtclient_mod_${id}.a"
  [[ -f "$lib" ]] || die "missing lib/libqtclient_mod_${id}.a"
  bash "$HERE/check-pack-zero-tip-imports.sh" --archive "$lib" --id "$id"
else
  mod="$(find "$mod_dir/native" -maxdepth 1 \( -name 'mod.dylib' -o -name 'mod.so' -o -name 'mod.dll' \) 2>/dev/null | head -1)"
  [[ -n "$mod" ]] || die "desktop pack must install mods/$id/native/mod.{dylib,so,dll}"
  bash "$HERE/check-pack-zero-tip-imports.sh" --mod "$mod"
fi
say "ok ($PLAT: $id built + installed from the SDK alone → $INSTALL)"
