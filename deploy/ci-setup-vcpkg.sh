#!/usr/bin/env bash
# Shared CI helper: pin VCPKG_ROOT, fetch manifest baseline, binary cache dirs.
# Prefer: source deploy/ci-setup-vcpkg.sh  (so VCPKG_ROOT is in the caller).
# Safe to bash when GITHUB_ENV is set (writes env for later steps).
set -euo pipefail

# MSVC / ilammy/msvc-dev-cmd sets VCPKG_ROOT to the VS-bundled tree, which has
# vcpkg.cmake but no .git — unusable for manifest baseline fetch. Prefer a real
# clone under $HOME/vcpkg (or VCPKG_FORCE_ROOT).
_default_vcpkg="${VCPKG_FORCE_ROOT:-$HOME/vcpkg}"
VCPKG_ROOT="${VCPKG_ROOT:-$_default_vcpkg}"
if ! git -C "$VCPKG_ROOT" rev-parse --git-dir >/dev/null 2>&1; then
  echo "[ci-vcpkg] ignoring non-git VCPKG_ROOT=$VCPKG_ROOT → $_default_vcpkg"
  VCPKG_ROOT="$_default_vcpkg"
fi
if [[ ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]]; then
  echo "[ci-vcpkg] cloning vcpkg into $VCPKG_ROOT"
  git clone --filter=blob:none https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
  if [[ "${OS:-}" == "Windows_NT" ]] || uname -s 2>/dev/null | grep -qiE 'mingw|msys|cygwin'; then
    if [[ -f "$VCPKG_ROOT/bootstrap-vcpkg.bat" ]]; then
      (cd "$VCPKG_ROOT" && cmd.exe //c bootstrap-vcpkg.bat -disableMetrics) || \
        "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
    else
      "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
    fi
  else
    "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
  fi
fi

# Manifest baseline must exist in the vcpkg git object store (shallow clone is not enough).
if [[ -f vcpkg.json ]]; then
  baseline="$(python3 -c 'import json;print(json.load(open("vcpkg.json")).get("builtin-baseline",""))' 2>/dev/null || true)"
  if [[ -n "$baseline" ]]; then
    if ! git -C "$VCPKG_ROOT" cat-file -e "${baseline}^{commit}" 2>/dev/null; then
      echo "[ci-vcpkg] fetching baseline $baseline"
      git -C "$VCPKG_ROOT" fetch --depth 1 origin "$baseline"
    fi
  fi
fi

export VCPKG_ROOT
export PATH="$VCPKG_ROOT:$PATH"

CACHE_ROOT="${VCPKG_BINARY_CACHE_DIR:-${RUNNER_TEMP:-/tmp}/vcpkg-binary-cache}"
DOWNLOADS="${VCPKG_DOWNLOADS:-${RUNNER_TEMP:-/tmp}/vcpkg-downloads}"
mkdir -p "$CACHE_ROOT" "$DOWNLOADS"
export VCPKG_DEFAULT_BINARY_CACHE="$CACHE_ROOT"
export VCPKG_BINARY_SOURCES="clear;files,$CACHE_ROOT,readwrite"
export VCPKG_DOWNLOADS="$DOWNLOADS"

if [[ -n "${GITHUB_ENV:-}" ]]; then
  {
    echo "VCPKG_ROOT=$VCPKG_ROOT"
    echo "VCPKG_DEFAULT_BINARY_CACHE=$CACHE_ROOT"
    echo "VCPKG_BINARY_SOURCES=$VCPKG_BINARY_SOURCES"
    echo "VCPKG_DOWNLOADS=$DOWNLOADS"
  } >> "$GITHUB_ENV"
fi

if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
  {
    echo "### vcpkg"
    echo "- VCPKG_ROOT=\`$VCPKG_ROOT\`"
    echo "- binary cache=\`$CACHE_ROOT\`"
    if [[ -f vcpkg.json ]]; then
      baseline="$(python3 -c 'import json;print(json.load(open("vcpkg.json")).get("builtin-baseline",""))' 2>/dev/null || true)"
      echo "- baseline=\`${baseline:0:12}\`"
    fi
  } >> "$GITHUB_STEP_SUMMARY"
fi

echo "[ci-vcpkg] VCPKG_ROOT=$VCPKG_ROOT cache=$CACHE_ROOT"
