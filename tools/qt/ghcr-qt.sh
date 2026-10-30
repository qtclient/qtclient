#!/usr/bin/env bash
# Pull / push CIP Qt install prefixes as OCI artifacts on GHCR.
#
#   tools/qt/ghcr-qt.sh pull <kit-slug>   # unpack into $QT_ROOT/cip-<sha>/<dir>
#   tools/qt/ghcr-qt.sh push <kit-slug>   # pack local prefix → ghcr.io/qtclient/qt-cip:<sha>-<kit>
#
# Requires: oras (https://oras.land). Auth: docker/oras login to ghcr.io
# (GITHUB_TOKEN with read:packages / write:packages in CI).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=qt-version.sh
source "$HERE/qt-version.sh"
QT_ROOT="$(qt_unix_path "${QT_ROOT:-$HOME/Qt}")"

CMD="${1:-}"
KIT="${2:-}"
[[ -n "$CMD" && -n "$KIT" ]] || {
  echo "usage: $0 pull|push|pull-release <kit-slug>" >&2
  exit 2
}

SHA="$(qt_cip_sha)"
[[ -n "$SHA" ]] || { echo "[qt-ghcr] no qt5-cip SHA (init submodule)" >&2; exit 1; }
SHORT="${SHA:0:12}"
TAG="${SHORT}-${KIT}"
REF="${QT_GHCR_IMAGE}:${TAG}"

kit_dir() {
  case "$KIT" in
    wasm_multithread) echo "wasm_multithread" ;;
    macos-arm64|macos-x64) echo "macos" ;;
    linux-x64|linux-arm64) echo "gcc_64" ;;
    windows-x64) echo "msvc2022_64" ;;
    *) echo "$KIT" ;;
  esac
}

DIR="$(kit_dir)"
PREFIX="$QT_ROOT/cip-$SHORT/$DIR"

need_oras() {
  command -v oras >/dev/null || {
    echo "[qt-ghcr] oras not found — install https://oras.land/docs/installation" >&2
    exit 1
  }
}

do_pull() {
  need_oras
  if [[ -f "$PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    echo "[qt-ghcr] already present: $PREFIX"
    return 0
  fi
  # Relative cwd path: Windows Git Bash `mktemp` (/tmp/…) vs native oras
  # disagree on absolute paths — same class of bug as push.
  local tmp=".qt-ghcr-pull-$$"
  rm -rf "$tmp"
  mkdir -p "$tmp"
  echo "[qt-ghcr] pulling $REF"
  if ! oras pull "$REF" -o "$tmp" 2>"$tmp/pull.err"; then
    echo "[qt-ghcr] pull miss: $REF ($(head -1 "$tmp/pull.err" 2>/dev/null || true))"
    rm -rf "$tmp"
    return 1
  fi
  # oras preserves the push path; older pushes used .qt-ghcr-push-*/qt-prefix.tar.zst
  local archive
  archive="$(find "$tmp" -type f \( -name '*.tar.zst' -o -name '*.tar.gz' -o -name 'qt-prefix.tar*' \) | head -1)"
  if [[ -z "$archive" ]]; then
    echo "[qt-ghcr] no archive in artifact" >&2
    rm -rf "$tmp"
    return 1
  fi
  mkdir -p "$PREFIX"
  case "$archive" in
    *.zst)
      # Prefer zstd|tar: Git Bash `tar --zstd` is unreliable on Windows runners.
      if command -v zstd >/dev/null; then
        zstd -d -c "$archive" | tar -xf - -C "$PREFIX"
      elif tar --zstd -xf "$archive" -C "$PREFIX"; then
        :
      else
        echo "[qt-ghcr] need zstd to unpack $archive (install zstd)" >&2
        rm -rf "$tmp"
        return 1
      fi
      ;;
    *) tar -xzf "$archive" -C "$PREFIX" ;;
  esac
  rm -rf "$tmp"
  [[ -f "$PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]] || {
    echo "[qt-ghcr] unpack missing Qt6Config.cmake" >&2
    return 1
  }
  echo "[qt-ghcr] installed $PREFIX"
}

do_push() {
  need_oras
  [[ -f "$PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]] || {
    echo "[qt-ghcr] nothing to push at $PREFIX" >&2
    exit 1
  }
  # Relative path under cwd: Windows Git Bash mktemp + native oras disagree on
  # absolute /tmp vs repo-relative paths (--disable-path-validation alone is not enough).
  local tmp archive layer_type
  tmp=".qt-ghcr-push-$$"
  rm -rf "$tmp"
  mkdir -p "$tmp"
  # shellcheck disable=SC2064
  trap "rm -rf '$tmp'" EXIT
  archive="$tmp/qt-prefix.tar.zst"
  echo "[qt-ghcr] packing $PREFIX"
  if command -v zstd >/dev/null; then
    tar -C "$PREFIX" -cf - . | zstd -T0 -3 -o "$archive"
  else
    archive="$tmp/qt-prefix.tar.gz"
    tar -C "$PREFIX" -czf "$archive" .
  fi
  echo "[qt-ghcr] pushing $REF"
  case "$archive" in
    *.zst) layer_type=application/vnd.oci.image.layer.v1.tar+zstd ;;
    *) layer_type=application/vnd.oci.image.layer.v1.tar+gzip ;;
  esac
  # Push basename-only so oras pull lands the archive at the extract root.
  local base
  base="$(basename "$archive")"
  (cd "$tmp" && oras push "$REF" \
    --artifact-type application/vnd.qtclient.qt.prefix.v1+tar \
    "${base}:${layer_type}")
  echo "[qt-ghcr] published $REF"
}


do_pull_release() {
  local rel="cip-qt-$SHORT" asset url
  if [[ -f "$PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    echo "[qt-ghcr] already present: $PREFIX"
    return 0
  fi
  asset="qt-cip-${TAG}.tar.zst"
  url="https://github.com/qtclient/qtclient/releases/download/${rel}/${asset}"
  echo "[qt-ghcr] pulling release $url"
  local tmp=".qt-ghcr-rel-$$"
  rm -rf "$tmp"
  mkdir -p "$tmp"
  if ! curl -fsSL "$url" -o "$tmp/$asset"; then
    asset="qt-cip-${TAG}.tar.gz"
    url="https://github.com/qtclient/qtclient/releases/download/${rel}/${asset}"
    curl -fsSL "$url" -o "$tmp/$asset" || {
      echo "[qt-ghcr] release miss: $url"
      rm -rf "$tmp"
      return 1
    }
  fi
  mkdir -p "$PREFIX"
  case "$asset" in
    *.zst)
      if command -v zstd >/dev/null; then
        zstd -d -c "$tmp/$asset" | tar -xf - -C "$PREFIX"
      else
        tar --zstd -xf "$tmp/$asset" -C "$PREFIX"
      fi
      ;;
    *) tar -xzf "$tmp/$asset" -C "$PREFIX" ;;
  esac
  rm -rf "$tmp"
  [[ -f "$PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]] || {
    echo "[qt-ghcr] release unpack missing Qt6Config.cmake" >&2
    return 1
  }
  echo "[qt-ghcr] installed from release $PREFIX"
}

case "$CMD" in
  pull) do_pull ;;
  push) do_push ;;
  pull-release) do_pull_release ;;
  *) echo "usage: $0 pull|push|pull-release <kit-slug>" >&2; exit 2 ;;
esac
