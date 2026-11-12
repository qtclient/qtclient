#!/usr/bin/env bash
# Shared CI helper: pin VCPKG_ROOT, fetch manifest baseline, binary cache dirs.
# Prefer: source scripts/ci/setup-vcpkg.sh  (so VCPKG_ROOT is in the caller).
# Safe to bash when GITHUB_ENV is set (writes env for later steps).
set -euo pipefail

_is_git_vcpkg() {
  local root="$1"
  [[ -f "$root/scripts/buildsystems/vcpkg.cmake" ]] \
    && git -C "$root" rev-parse --git-dir >/dev/null 2>&1
}

_is_partial_clone() {
  git -C "$1" config --get remote.origin.partialclonefilter >/dev/null 2>&1
}

# MSVC / ilammy/msvc-dev-cmd sets VCPKG_ROOT to the VS-bundled tree (cmake
# toolchain, no .git). GitHub windows runners also ship C:\vcpkg as a full
# clone — prefer that, then $HOME/vcpkg / VCPKG_FORCE_ROOT.
_default_vcpkg="${VCPKG_FORCE_ROOT:-$HOME/vcpkg}"
for candidate in \
  "${VCPKG_FORCE_ROOT:-}" \
  "/c/vcpkg" \
  "C:/vcpkg" \
  "${VCPKG_ROOT:-}" \
  "$_default_vcpkg"
do
  [[ -n "$candidate" ]] || continue
  if _is_git_vcpkg "$candidate" && ! _is_partial_clone "$candidate"; then
    VCPKG_ROOT="$candidate"
    break
  fi
done
VCPKG_ROOT="${VCPKG_ROOT:-$_default_vcpkg}"

if ! _is_git_vcpkg "$VCPKG_ROOT" || _is_partial_clone "$VCPKG_ROOT"; then
  # Partial clones (--filter=blob:none) break vcpkg port checkouts on Windows
  # Git ("Could not resolve host" / promisor fetch). Always use a full clone.
  if [[ -e "$VCPKG_ROOT" ]]; then
    echo "[ci-vcpkg] replacing unusable tree at $VCPKG_ROOT"
    rm -rf "$VCPKG_ROOT"
  fi
  VCPKG_ROOT="$_default_vcpkg"
  echo "[ci-vcpkg] cloning full vcpkg into $VCPKG_ROOT"
  git clone https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
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

# Manifest baseline must exist in the vcpkg git object store.
if [[ -f vcpkg.json ]]; then
  baseline="$(python3 -c 'import json;print(json.load(open("vcpkg.json")).get("builtin-baseline",""))' 2>/dev/null || true)"
  if [[ -n "$baseline" ]]; then
    if ! git -C "$VCPKG_ROOT" cat-file -e "${baseline}^{commit}" 2>/dev/null; then
      echo "[ci-vcpkg] fetching baseline $baseline"
      git -C "$VCPKG_ROOT" fetch --depth 1 origin "$baseline"
    fi
  fi
fi

# CMake on Windows wants a drive path; keep Unix form for bash callers that
# already tolerate /c/... (cmake accepts both via the toolchain include).
if command -v cygpath >/dev/null 2>&1; then
  case "$VCPKG_ROOT" in
    [A-Za-z]:*|*\\*|/*)
      VCPKG_ROOT="$(cygpath -m "$VCPKG_ROOT")"
      ;;
  esac
fi

export VCPKG_ROOT
export PATH="$VCPKG_ROOT:$PATH"

# Prefer workspace/.cache/vcpkg (composite cache-restore) over ephemeral RUNNER_TEMP.
_default_cache="${GITHUB_WORKSPACE:+$GITHUB_WORKSPACE/.cache/vcpkg}"
_default_cache="${_default_cache:-${RUNNER_TEMP:-/tmp}/vcpkg-binary-cache}"
CACHE_ROOT="${VCPKG_BINARY_CACHE_DIR:-$_default_cache}"
DOWNLOADS="${VCPKG_DOWNLOADS:-${RUNNER_TEMP:-/tmp}/vcpkg-downloads}"
mkdir -p "$CACHE_ROOT" "$DOWNLOADS"
export VCPKG_BINARY_CACHE_DIR="$CACHE_ROOT"
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
