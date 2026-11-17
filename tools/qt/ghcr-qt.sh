#!/usr/bin/env bash
# Pull the qtclient Qt kits (published by qtclient/Qt5 for each commit).
#
#   tools/qt/ghcr-qt.sh pull <kit-slug>           # ghcr.io/qtclient/qt5:<sha12>-<kit>
#   tools/qt/ghcr-qt.sh pull-release <kit-slug>   # qtclient/Qt5 release qt-<sha12>
#
# Both unpack into $QT_ROOT/cip-<sha12>/<dir>. GHCR needs oras
# (https://oras.land); kits are .tar.zst (zstd).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
# shellcheck source=qt-version.sh
source "$HERE/qt-version.sh"
QT_ROOT="$(qt_unix_path "${QT_ROOT:-$HOME/Qt}")"

CMD="${1:-}"
KIT="${2:-}"
[[ -n "$CMD" && -n "$KIT" ]] || {
  echo "usage: $0 pull|pull-release <kit-slug>" >&2
  exit 2
}

SHA="$(qt_resolve_cip_sha)"
[[ -n "$SHA" ]] || { echo "[qt-ghcr] no qt5-cip SHA (init submodule)" >&2; exit 1; }
SHORT="${SHA:0:12}"
TAG="${SHORT}-${KIT}"
REF="${QT_GHCR_IMAGE}:${TAG}"
DIR="$(qt_install_dir_for_kit "$KIT")"
PREFIX="$QT_ROOT/cip-$SHORT/$DIR"

need_oras() {
  command -v oras >/dev/null || {
    echo "[qt-ghcr] oras not found — install https://oras.land/docs/installation" >&2
    exit 1
  }
}

# Prefer flat qt-prefix.tar.zst; accept legacy nested paths.
_find_archive() {
  local root="$1" flat
  flat="$(find "$root" -maxdepth 1 -type f \( -name 'qt-prefix.tar.zst' -o -name 'qt-prefix.tar.gz' \) | head -1)"
  if [[ -n "$flat" ]]; then
    echo "$flat"
    return 0
  fi
  find "$root" -type f \( -name '*.tar.zst' -o -name '*.tar.gz' -o -name 'qt-prefix.tar*' \) | head -1
}

do_pull() {
  need_oras
  if qt_prefix_has_qt6 "$PREFIX"; then
    echo "[qt-ghcr] already present: $PREFIX"
    return 0
  fi
  # Relative cwd path: Windows Git Bash mktemp (/tmp/…) vs native oras disagree.
  local tmp=".qt-ghcr-pull-$$"
  rm -rf "$tmp"
  mkdir -p "$tmp"
  echo "[qt-ghcr] pulling $REF"
  if ! oras pull "$REF" -o "$tmp" 2>"$tmp/pull.err"; then
    echo "[qt-ghcr] pull miss: $REF ($(head -1 "$tmp/pull.err" 2>/dev/null || true))"
    rm -rf "$tmp"
    return 1
  fi
  local archive
  archive="$(_find_archive "$tmp")"
  if [[ -z "$archive" ]]; then
    echo "[qt-ghcr] no archive in artifact" >&2
    rm -rf "$tmp"
    return 1
  fi
  if ! qt_unpack_prefix_archive "$archive" "$PREFIX"; then
    rm -rf "$tmp"
    return 1
  fi
  rm -rf "$tmp"
  if ! qt_prefix_has_qt6 "$PREFIX"; then
    echo "[qt-ghcr] unpack missing Qt6Config.cmake" >&2
    return 1
  fi
  echo "[qt-ghcr] installed $PREFIX"
}

do_pull_release() {
  local rel="qt-$SHORT" asset url tmp
  if qt_prefix_has_qt6 "$PREFIX"; then
    echo "[qt-ghcr] already present: $PREFIX"
    return 0
  fi
  asset="qt-${TAG}.tar.zst"
  url="https://github.com/qtclient/Qt5/releases/download/${rel}/${asset}"
  echo "[qt-ghcr] pulling release $url"
  tmp=".qt-ghcr-rel-$$"
  rm -rf "$tmp"
  mkdir -p "$tmp"
  if ! curl -fsSL "$url" -o "$tmp/$asset"; then
    asset="qt-${TAG}.tar.gz"
    url="https://github.com/qtclient/Qt5/releases/download/${rel}/${asset}"
    curl -fsSL "$url" -o "$tmp/$asset" || {
      echo "[qt-ghcr] release miss: $url (published non-draft qt-* required)"
      rm -rf "$tmp"
      return 1
    }
  fi
  if ! qt_unpack_prefix_archive "$tmp/$asset" "$PREFIX"; then
    rm -rf "$tmp"
    return 1
  fi
  rm -rf "$tmp"
  if ! qt_prefix_has_qt6 "$PREFIX"; then
    echo "[qt-ghcr] release unpack missing Qt6Config.cmake" >&2
    return 1
  fi
  echo "[qt-ghcr] installed from release $PREFIX"
}

case "$CMD" in
  pull) do_pull ;;
  pull-release) do_pull_release ;;
  *) echo "usage: $0 pull|pull-release <kit-slug>" >&2; exit 2 ;;
esac
