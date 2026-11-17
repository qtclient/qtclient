#!/usr/bin/env bash
# Install the qtclient Qt kits into $QT_ROOT. Pull only: kits are built and
# published by qtclient/Qt5 (our fork of CipSoft-Tibia/Qt5, third_party/qt5-cip)
# for each of its commits ("qtclient kits" workflow, qtclient/build-kit.sh there).
#
#   tools/qt/install-qt.sh              host desktop kit
#   tools/qt/install-qt.sh --wasm       + wasm_multithread kit + its emsdk
#   --pull-only / QT_PULL_ONLY=1        wait up to QT_PULL_WAIT_SECS for a kit
#                                       that is still being built
#
# Resolve order: local prefix → GHCR → GitHub Release → fail.
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
# Seconds to retry pulls with --pull-only (default 90m: a kit build in progress).
QT_PULL_WAIT_SECS="${QT_PULL_WAIT_SECS:-5400}"
QT_PULL_INTERVAL_SECS="${QT_PULL_INTERVAL_SECS:-60}"
for arg in "$@"; do
  case "$arg" in
    --wasm) WASM=1 ;;
    --pull-only) PULL_ONLY=1 ;;
    -h|--help)
      echo "Usage: $0 [--wasm] [--pull-only]"
      echo "  Pulls the qtclient Qt kits for the pinned third_party/qt5-cip commit."
      echo "  --pull-only / QT_PULL_ONLY=1: wait up to QT_PULL_WAIT_SECS, then fail."
      exit 0
      ;;
    *)
      echo "[qt] unknown argument: $arg" >&2
      exit 1
      ;;
  esac
done

ensure_prefix() {
  local kit_slug="$1" dir_name="$2"
  local sha short prefix
  sha="$(qt_cip_sha)"
  [[ -n "$sha" ]] || { echo "[qt] no CIP SHA" >&2; exit 1; }
  short="${sha:0:12}"
  prefix="$QT_ROOT/cip-$short/$dir_name"
  if qt_prefix_has_qt6 "$prefix"; then
    echo "[qt] $kit_slug: $prefix" >&2
    echo "$prefix"
    return 0
  fi
  if bash "$HERE/ghcr-qt.sh" pull "$kit_slug" >&2; then
    echo "$prefix"
    return 0
  fi
  # Public GitHub Release mirror (the product release repo cannot read private GHCR packages).
  if bash "$HERE/ghcr-qt.sh" pull-release "$kit_slug" >&2; then
    echo "$prefix"
    return 0
  fi
  return 1
}

# Retry until the deadline (the kit may still be building), then fail.
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
  echo "[qt]   kits come from qtclient/Qt5 \"qtclient kits\" for the pinned third_party/qt5-cip commit" >&2
  return 1
}

fail_missing_kit() {
  local kit_slug="$1"
  echo "[qt] missing Qt kit '$kit_slug' (local + GHCR + release mirror)" >&2
  echo "[qt]   image ${QT_GHCR_IMAGE}:<sha12>-${kit_slug}" >&2
  echo "[qt]   built by qtclient/Qt5 \"qtclient kits\" for the pinned third_party/qt5-cip commit" >&2
  return 1
}

kit_prefix() {
  local kit_slug="$1" dir_name="$2"
  if [[ "$PULL_ONLY" -eq 1 ]]; then
    ensure_prefix_or_wait "$kit_slug" "$dir_name"
  else
    ensure_prefix "$kit_slug" "$dir_name" || fail_missing_kit "$kit_slug"
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

# True when installed wasm kit has native EH + JSPI (required for unified qtclient-fs).
wasm_kit_has_jspi() {
  local prefix="$1" cfg
  cfg="$prefix/include/QtCore/qconfig.h"
  [[ -f "$cfg" ]] || return 1
  grep -qE '^#define QT_FEATURE_wasm_exceptions 1$' "$cfg" || return 1
  grep -qE '^#define QT_FEATURE_wasm_jspi 1$' "$cfg" || return 1
  return 0
}

mkdir -p "$QT_ROOT"
host="$(kit_prefix "$(qt_kit_slug)" "$(qt_host_dir_name)")" || exit 1
echo "[qt] desktop: $host"
if [[ "$WASM" -eq 1 ]]; then
  wasm="$(kit_prefix wasm_multithread wasm_multithread)" || exit 1
  if ! wasm_kit_has_jspi "$wasm"; then
    echo "[qt] wasm kit at $wasm lacks wasm-exceptions/jspi" >&2
    exit 1
  fi
  # The kit records the Emscripten it was built with; build with the same.
  if [[ -f "$wasm/emsdk-version" && "$(cat "$wasm/emsdk-version")" != "$QT_EMSDK_VERSION" ]]; then
    echo "[qt] wasm kit was built with emsdk $(cat "$wasm/emsdk-version"); QT_EMSDK_VERSION is $QT_EMSDK_VERSION" >&2
    exit 1
  fi
  ensure_emsdk
  echo "[qt] wasm: $wasm"
fi
