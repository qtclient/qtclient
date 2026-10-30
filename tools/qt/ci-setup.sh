#!/usr/bin/env bash
# Single qtclient / pack CI entrypoint for CIP Qt (+ optional deps/vcpkg).
#
#   tools/qt/ci-setup.sh [--wasm] [--pull-only] [--with-vcpkg] [--with-deps]
#
# Always pull-only for CIP Qt (never --build). Compiling Qt belongs in Qt: build.
# Env: QT_ROOT, QT_CIP_SHA, QT_PULL_*, GITHUB_TOKEN / GITHUB_ACTOR
# Writes GITHUB_ENV when set: QT_ROOT QT_CIP_SHA QT_CIP_SHORT QT_PREFIX [VCPKG_*]
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="$(cd "$HERE/../.." && pwd)"
cd "$OTC"

# shellcheck source=qt-version.sh
source "$HERE/qt-version.sh"

WASM=0
# Consumer CI always waits/fails on miss — never falls through to a Qt compile.
PULL_ONLY=1
WITH_VCPKG=0
WITH_DEPS=0
[[ "${QT_PULL_ONLY:-0}" == "1" ]] && PULL_ONLY=1
for arg in "$@"; do
  case "$arg" in
    --wasm) WASM=1 ;;
    --pull-only) PULL_ONLY=1 ;; # explicit; already the default
    --with-vcpkg) WITH_VCPKG=1 ;;
    --with-deps) WITH_DEPS=1 ;;
    -h|--help)
      echo "usage: $0 [--wasm] [--pull-only] [--with-vcpkg] [--with-deps]"
      echo "  Always pulls CIP Qt (no source build). Use Soft Qt: build to compile."
      exit 0
      ;;
    *) echo "unknown: $arg" >&2; exit 2 ;;
  esac
done

install_deps() {
  case "$(uname -s)" in
    Linux)
      sudo apt-get update -qq
      sudo apt-get install -y -qq build-essential ninja-build ccache zstd \
        libgl1-mesa-dev libxkbcommon-dev libfontconfig1-dev patchelf \
        libegl1-mesa-dev gzip brotli git curl zip unzip tar || \
      sudo apt-get install -y -qq ninja-build ccache zstd \
        libgl1-mesa-dev libxkbcommon-dev libfontconfig1-dev
      ;;
    Darwin)
      brew install ninja ccache zstd
      ;;
    MINGW*|MSYS*|CYGWIN*)
      # Chocolatey id is zstandard (zstd.exe). Also prefer C:\tools\zstd on runners.
      choco install ccache ninja zstandard -y --no-progress || true
      export PATH="/c/ProgramData/chocolatey/bin:/c/tools/zstd:$PATH"
      if [[ -n "${GITHUB_PATH:-}" ]]; then
        echo "/c/ProgramData/chocolatey/bin" >> "$GITHUB_PATH"
        echo "/c/tools/zstd" >> "$GITHUB_PATH"
      fi
      command -v zstd >/dev/null || {
        echo "::error::zstd missing after choco install zstandard" >&2
        exit 1
      }
      ;;
    *)
      echo "[qt-ci] unknown OS for --with-deps: $(uname -s)" >&2
      exit 1
      ;;
  esac
}

if [[ "$WITH_DEPS" == "1" ]]; then
  install_deps
fi

git submodule update --init third_party/Qt5LegacyControls

SHA="$(qt_resolve_cip_sha)"
if [[ -z "$SHA" && "$PULL_ONLY" -eq 0 ]]; then
  git -c submodule.third_party/qt5-cip.update=checkout \
    submodule update --init --depth 1 third_party/qt5-cip || true
  SHA="$(qt_resolve_cip_sha)"
fi
[[ -n "$SHA" ]] || { echo "[qt-ci] no qt5-cip SHA" >&2; exit 1; }
export QT_CIP_SHA="$SHA"
echo "[qt-ci] CIP SHA $SHA pull_only=$PULL_ONLY wasm=$WASM"

QT_ROOT="$(qt_unix_path "${QT_ROOT:-$HOME/Qt}")"
export QT_ROOT

if [[ -n "${GITHUB_ENV:-}" ]]; then
  {
    echo "QT_CIP_SHA=$SHA"
    echo "QT_CIP_SHORT=${SHA:0:12}"
    echo "QT_ROOT=$QT_ROOT"
  } >> "$GITHUB_ENV"
fi

if [[ "$WITH_VCPKG" == "1" ]]; then
  # shellcheck source=../../scripts/ci/setup-vcpkg.sh
  source "$OTC/scripts/ci/setup-vcpkg.sh"
fi

if [[ -n "${GITHUB_TOKEN:-}" ]] && command -v oras >/dev/null; then
  echo "$GITHUB_TOKEN" | oras login ghcr.io -u "${GITHUB_ACTOR:-github-actions}" --password-stdin || true
fi

ARGS=()
[[ "$WASM" == "1" ]] && ARGS+=(--wasm)
[[ "$PULL_ONLY" == "1" ]] && ARGS+=(--pull-only)
if ((${#ARGS[@]})); then
  bash "$HERE/install-qt.sh" "${ARGS[@]}"
else
  bash "$HERE/install-qt.sh"
fi

if [[ "$WASM" == "1" ]]; then
  PREFIX="$(bash "$HERE/qt-prefix.sh" --wasm)"
else
  PREFIX="$(bash "$HERE/qt-prefix.sh")"
fi
[[ -n "$PREFIX" ]] || { echo "[qt-ci] empty QT_PREFIX" >&2; exit 1; }
if [[ -n "${GITHUB_ENV:-}" ]]; then
  echo "QT_PREFIX=$PREFIX" >> "$GITHUB_ENV"
fi
echo "[qt-ci] QT_PREFIX=$PREFIX"
