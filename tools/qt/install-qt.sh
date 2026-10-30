#!/usr/bin/env bash
# Install CipSoft-patched Qt from third_party/qt5-cip into $QT_ROOT.
#
#   tools/qt/install-qt.sh            host desktop kit
#   tools/qt/install-qt.sh --wasm     + wasm_multithread kit + emsdk
#
# Resolve order per kit: local prefix → GHCR pull → (unless --pull-only) build.
# Set PUBLISH=1 after a successful build to push the prefix to GHCR.
# QT_PULL_ONLY=1 / --pull-only: never compile Qt; wait for GHCR then fail.
#
# Portable to macOS /bin/bash 3.2: no `local var=()`, no `local -a`, no
# `read -a` — those parse/`set -u` differently and break after long builds.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=qt-version.sh
source "$HERE/qt-version.sh"

QT_ROOT="$(qt_unix_path "${QT_ROOT:-$HOME/Qt}")"
WASM=0
PULL_ONLY=0
[[ "${QT_PULL_ONLY:-0}" == "1" ]] && PULL_ONLY=1
# Seconds to retry GHCR pulls when --pull-only (default 90m for concurrent Build CIP Qt).
QT_PULL_WAIT_SECS="${QT_PULL_WAIT_SECS:-5400}"
QT_PULL_INTERVAL_SECS="${QT_PULL_INTERVAL_SECS:-60}"

for arg in "$@"; do
  case "$arg" in
    --wasm) WASM=1 ;;
    --pull-only) PULL_ONLY=1 ;;
    -h|--help)
      echo "Usage: $0 [--wasm] [--pull-only]"
      echo "  Default: local → GHCR → build from third_party/qt5-cip (PUBLISH=1 pushes)."
      echo "  --pull-only / QT_PULL_ONLY=1: never build; wait up to QT_PULL_WAIT_SECS for GHCR."
      exit 0
      ;;
    *)
      echo "[qt] unknown argument: $arg" >&2
      exit 1
      ;;
  esac
done

OTC="$(cd "$HERE/../.." && pwd)"
SRC="$QT_CIP_SRC"
JOBS="${CMAKE_BUILD_PARALLEL_LEVEL:-$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)}"

# Optional extra configure argv (strings, word-split intentionally).
HOST_PLATFORM_EXTRA=""
HOST_CMAKE_EXTRA=""

qt_configure() {
  if [[ -x "$SRC/configure" ]]; then
    echo "$SRC/configure"
  elif [[ -f "$SRC/configure.bat" ]]; then
    echo "$SRC/configure.bat"
  else
    echo "[qt] no configure in $SRC" >&2
    exit 1
  fi
}

init_submodule() {
  if [[ ! -f "$SRC/configure" && ! -f "$SRC/configure.bat" ]]; then
    local sha
    sha="$(qt_cip_sha)"
    echo "[qt] initializing third_party/qt5-cip (shallow${sha:+ @ ${sha:0:12}})…"
    if git -C "$OTC" config -f .gitmodules --get-regexp 'submodule\..*\.path' 2>/dev/null | grep -q 'third_party/qt5-cip'; then
      git -C "$OTC" -c submodule.third_party/qt5-cip.update=checkout \
        submodule update --init --depth 1 third_party/qt5-cip || true
    fi
    if [[ ! -f "$SRC/configure" && ! -f "$SRC/configure.bat" ]]; then
      rm -rf "$SRC"
      mkdir -p "$(dirname "$SRC")"
      git clone --depth 1 https://github.com/CipSoft-Tibia/Qt5.git "$SRC"
      if [[ -n "$sha" ]]; then
        git -C "$SRC" fetch --depth 1 origin "$sha" 2>/dev/null || true
        git -C "$SRC" checkout "$sha" 2>/dev/null || git -C "$SRC" checkout FETCH_HEAD 2>/dev/null || true
      fi
    fi
  fi
  [[ -f "$SRC/configure" || -f "$SRC/configure.bat" ]] || {
    echo "[qt] qt5-cip missing after submodule init" >&2
    exit 1
  }
  # Drop skipped module trees so IDE git status stays usable (~0 changes).
  qt_cip_sparse_checkout "$SRC"
}

ensure_prefix() {
  local kit_slug="$1" dir_name="$2"
  local sha short prefix
  sha="$(qt_cip_sha)"
  [[ -n "$sha" ]] || { echo "[qt] no CIP SHA" >&2; exit 1; }
  short="${sha:0:12}"
  prefix="$QT_ROOT/cip-$short/$dir_name"
  if [[ -f "$prefix/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    echo "[qt] $kit_slug: $prefix" >&2
    echo "$prefix"
    return 0
  fi
  if bash "$HERE/ghcr-qt.sh" pull "$kit_slug" >&2; then
    echo "$prefix"
    return 0
  fi
  # Public GitHub Release mirror (idleot/release cannot read private GHCR packages).
  if bash "$HERE/ghcr-qt.sh" pull-release "$kit_slug" >&2; then
    echo "$prefix"
    return 0
  fi
  return 1
}

# Pull-only: retry GHCR until deadline, then fail (never compile).
ensure_prefix_or_wait() {
  local kit_slug="$1" dir_name="$2"
  local deadline now prefix
  if prefix="$(ensure_prefix "$kit_slug" "$dir_name")"; then
    echo "$prefix"
    return 0
  fi
  deadline=$((SECONDS + QT_PULL_WAIT_SECS))
  echo "[qt] pull-only: waiting up to ${QT_PULL_WAIT_SECS}s for ghcr.io artifact ($kit_slug)…" >&2
  while (( SECONDS < deadline )); do
    sleep "$QT_PULL_INTERVAL_SECS"
    if prefix="$(ensure_prefix "$kit_slug" "$dir_name")"; then
      echo "$prefix"
      return 0
    fi
    now=$((deadline - SECONDS))
    echo "[qt] still waiting for $kit_slug (${now}s left)…" >&2
  done
  echo "[qt] pull-only: miss for $kit_slug after ${QT_PULL_WAIT_SECS}s" >&2
  echo "[qt]   tried GHCR ${QT_GHCR_IMAGE}:<sha12>-${kit_slug} and GitHub Release qt-<sha12>" >&2
  echo "[qt]   run Build CIP Qt + Publish CIP Qt release assets (non-draft), then retry" >&2
  return 1
}

# Sets global SKIP_ARGS="-skip mod -skip …" from QT_SKIP_MODULES.
build_skip_args() {
  SKIP_ARGS=""
  local m
  for m in $QT_SKIP_MODULES; do
    SKIP_ARGS="$SKIP_ARGS -skip $m"
  done
}

# GitHub windows-2022 + Git Bash puts MinGW ahead of MSVC; CMake then picks
# GNU and qsb.exe dies at runtime (0xC0000005 / 0xC0000139). Prefer cl.exe.
windows_prefer_msvc() {
  case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*) ;;
    *)
      [[ "${OS:-}" == "Windows_NT" ]] || return 0
      ;;
  esac
  local cleaned="" part oifs
  oifs="$IFS"
  IFS=':'
  # shellcheck disable=SC2086
  for part in $PATH; do
    case "$part" in
      *[Mm]ingw*|*/mingw64/*|*/mingw32/*) continue ;;
    esac
    if [[ -n "$cleaned" ]]; then
      cleaned="$cleaned:$part"
    else
      cleaned="$part"
    fi
  done
  IFS="$oifs"
  PATH="$cleaned"
  export PATH
  if ! command -v cl >/dev/null 2>&1 && ! command -v cl.exe >/dev/null 2>&1; then
    echo "[qt] Windows: cl.exe not on PATH (need VS Build Tools / ilammy/msvc-dev-cmd)" >&2
    exit 1
  fi
  echo "[qt] Windows: using MSVC ($(command -v cl.exe 2>/dev/null || command -v cl))"
  HOST_PLATFORM_EXTRA="-platform win32-msvc"
  HOST_CMAKE_EXTRA="-DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl"
}

build_host() {
  local kit_slug dir_name sha short prefix build
  HOST_PLATFORM_EXTRA=""
  HOST_CMAKE_EXTRA=""
  kit_slug="$(qt_kit_slug)"
  dir_name="$(qt_host_dir_name)"
  if [[ "$PULL_ONLY" -eq 1 ]]; then
    prefix="$(ensure_prefix_or_wait "$kit_slug" "$dir_name")" || exit 1
    echo "[qt] desktop (pull-only): $prefix"
    return 0
  fi
  if prefix="$(ensure_prefix "$kit_slug" "$dir_name")"; then
    # Cache/local hit still publishes so idleot/release can pull from GHCR.
    if [[ "${PUBLISH:-0}" == "1" ]]; then
      bash "$HERE/ghcr-qt.sh" push "$kit_slug"
    fi
    return 0
  fi
  windows_prefer_msvc
  init_submodule
  sha="$(qt_cip_sha)"
  short="${sha:0:12}"
  prefix="$QT_ROOT/cip-$short/$dir_name"
  build="$QT_ROOT/build/cip-$short-host"
  mkdir -p "$prefix" "$build"
  echo "[qt] configuring host CIP Qt $short → $prefix"
  build_skip_args
  (
    cd "$build"
    if [[ ! -f CMakeCache.txt ]]; then
      # Intentional word-split of EXTRA / SKIP_ARGS (bash 3.2-safe, no arrays).
      # shellcheck disable=SC2086
      "$(qt_configure)" \
        -prefix "$prefix" \
        -release \
        -nomake examples \
        -nomake tests \
        -opensource -confirm-license \
        $HOST_PLATFORM_EXTRA \
        $SKIP_ARGS \
        -- -DCMAKE_BUILD_TYPE=Release $HOST_CMAKE_EXTRA
    fi
    cmake --build . --parallel "$JOBS"
    cmake --install .
  )
  [[ -f "$prefix/lib/cmake/Qt6/Qt6Config.cmake" ]] || {
    echo "[qt] host install failed" >&2
    exit 1
  }
  echo "[qt] desktop: $prefix"
  if [[ "${PUBLISH:-0}" == "1" ]]; then
    bash "$HERE/ghcr-qt.sh" push "$kit_slug"
  fi
}

ensure_emsdk() {
  # Do not name a local `EMSDK` — sourcing emsdk_env.sh exports EMSDK, and a
  # local of that name shadows it so the env is gone after this function returns
  # (Qt wasm configure then fails: "Can't find an Emscripten SDK").
  local emsdk_root="$QT_ROOT/emsdk"
  if [[ ! -x "$emsdk_root/emsdk" ]]; then
    git clone --filter=blob:none https://github.com/emscripten-core/emsdk.git "$emsdk_root"
  fi
  if [[ "$(cat "$emsdk_root/.emsdk_version" 2>/dev/null || true)" != "$QT_EMSDK_VERSION" ]]; then
    "$emsdk_root/emsdk" install "$QT_EMSDK_VERSION"
    "$emsdk_root/emsdk" activate "$QT_EMSDK_VERSION" >/dev/null
    echo "$QT_EMSDK_VERSION" > "$emsdk_root/.emsdk_version"
  fi
  # shellcheck disable=SC1091
  source "$emsdk_root/emsdk_env.sh"
  export EMSDK="${EMSDK:-$emsdk_root}"
  command -v emcc >/dev/null || {
    echo "[qt] emcc not on PATH after sourcing $emsdk_root/emsdk_env.sh" >&2
    exit 1
  }
  echo "[qt] emsdk $QT_EMSDK_VERSION at $EMSDK (emcc=$(command -v emcc))"
}

build_wasm() {
  local kit_slug="wasm_multithread" dir_name="wasm_multithread"
  local sha short prefix host_prefix build
  if [[ "$PULL_ONLY" -eq 1 ]]; then
    prefix="$(ensure_prefix_or_wait "$kit_slug" "$dir_name")" || exit 1
    ensure_emsdk
    echo "[qt] wasm (pull-only): $prefix"
    return 0
  fi
  if prefix="$(ensure_prefix "$kit_slug" "$dir_name")"; then
    ensure_emsdk
    if [[ "${PUBLISH:-0}" == "1" ]]; then
      bash "$HERE/ghcr-qt.sh" push "$kit_slug"
    fi
    return 0
  fi
  build_host
  ensure_emsdk
  init_submodule
  sha="$(qt_cip_sha)"
  short="${sha:0:12}"
  host_prefix="$QT_ROOT/cip-$short/$(qt_host_dir_name)"
  prefix="$QT_ROOT/cip-$short/$dir_name"
  build="$QT_ROOT/build/cip-$short-wasm"
  mkdir -p "$prefix" "$build"
  echo "[qt] configuring wasm CIP Qt $short → $prefix (host $host_prefix)"
  build_skip_args
  (
    cd "$build"
    if [[ ! -f CMakeCache.txt ]]; then
      # shellcheck disable=SC2086
      "$(qt_configure)" \
        -prefix "$prefix" \
        -qt-host-path "$host_prefix" \
        -platform wasm-emscripten \
        -feature-thread \
        -release \
        -nomake examples \
        -nomake tests \
        -opensource -confirm-license \
        $SKIP_ARGS \
        -- -DCMAKE_BUILD_TYPE=Release
    fi
    cmake --build . --parallel "$JOBS"
    cmake --install .
  )
  [[ -f "$prefix/lib/cmake/Qt6/Qt6Config.cmake" ]] || {
    echo "[qt] wasm install failed" >&2
    exit 1
  }
  echo "[qt] wasm: $prefix"
  if [[ "${PUBLISH:-0}" == "1" ]]; then
    bash "$HERE/ghcr-qt.sh" push "$kit_slug"
  fi
}

mkdir -p "$QT_ROOT"
build_host
if [[ "$WASM" -eq 1 ]]; then
  build_wasm
fi
