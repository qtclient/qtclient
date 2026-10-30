#!/usr/bin/env bash
# CI helper: init Qt5LegacyControls, then install-qt.sh.
# Usage: tools/qt/ci-setup-qt.sh [--wasm] [--pull-only]
# Env: QT_ROOT, PUBLISH, QT_PULL_ONLY, GITHUB_TOKEN / GITHUB_ACTOR for oras login
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="$(cd "$HERE/../.." && pwd)"
cd "$OTC"

WASM=0
PULL_ONLY=0
[[ "${QT_PULL_ONLY:-0}" == "1" ]] && PULL_ONLY=1
for arg in "$@"; do
  case "$arg" in
    --wasm) WASM=1 ;;
    --pull-only) PULL_ONLY=1 ;;
    *) echo "unknown: $arg" >&2; exit 2 ;;
  esac
done

git submodule update --init third_party/Qt5LegacyControls

# shellcheck source=qt-version.sh
source "$HERE/qt-version.sh"
SHA="$(qt_cip_sha)"
if [[ -z "$SHA" && "$PULL_ONLY" -eq 0 ]]; then
  git -c submodule.third_party/qt5-cip.update=checkout \
    submodule update --init --depth 1 third_party/qt5-cip || true
  SHA="$(qt_cip_sha)"
fi
echo "[qt-ci] CIP SHA ${SHA:-unknown} pull_only=$PULL_ONLY"
if [[ -n "${GITHUB_ENV:-}" && -n "$SHA" ]]; then
  echo "CIP_SHA=$SHA" >> "$GITHUB_ENV"
  echo "CIP_SHORT=${SHA:0:12}" >> "$GITHUB_ENV"
fi
# Persist QT_ROOT so later steps (build-tibia.sh → qt-prefix.sh) find the kit.
if [[ -n "${GITHUB_ENV:-}" ]]; then
  echo "QT_ROOT=${QT_ROOT:-$HOME/Qt}" >> "$GITHUB_ENV"
fi

if [[ -n "${GITHUB_TOKEN:-}" ]] && command -v oras >/dev/null; then
  echo "$GITHUB_TOKEN" | oras login ghcr.io -u "${GITHUB_ACTOR:-github-actions}" --password-stdin || true
fi

ARGS=()
[[ "$WASM" == "1" ]] && ARGS+=(--wasm)
[[ "$PULL_ONLY" == "1" ]] && ARGS+=(--pull-only)
# Empty "${ARGS[@]}" is unbound under `set -u` on bash 3 / some CI images.
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
