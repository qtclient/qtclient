#!/usr/bin/env bash
# Download a Soft host ccache archive published by qtclient/qtclient "CI host".
#
#   scripts/ci/fetch-qtclient-ccache.sh --sha <qtclient_commit> --os linux|macos|windows
#   export CCACHE_DIR=$PWD/.ccache
#
# Requires: gh (authenticated), zstd, tar.
set -euo pipefail

REPO="${QTCLIENT_REPO:-qtclient/qtclient}"
SHA=""
OS_SLUG=""
OUT_DIR="${CCACHE_DIR:-$PWD/.ccache}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --sha) SHA="$2"; shift 2 ;;
    --os) OS_SLUG="$2"; shift 2 ;;
    --repo) REPO="$2"; shift 2 ;;
    --out) OUT_DIR="$2"; shift 2 ;;
    -h|--help)
      echo "usage: $0 --sha <commit> --os linux|macos|windows [--repo qtclient/qtclient] [--out DIR]"
      exit 0
      ;;
    *) echo "unknown: $1" >&2; exit 2 ;;
  esac
done

[[ -n "$SHA" && -n "$OS_SLUG" ]] || {
  echo "need --sha and --os" >&2
  exit 2
}
command -v gh >/dev/null || { echo "gh CLI required" >&2; exit 1; }
command -v zstd >/dev/null || { echo "zstd required" >&2; exit 1; }

ART="ccache-${OS_SLUG}-${SHA}"
TMP="$(mktemp -d)"
# shellcheck disable=SC2064
trap "rm -rf '$TMP'" EXIT

echo "[qtclient-ccache] fetching artifact $ART from $REPO @ $SHA"
if ! gh run download -R "$REPO" -n "$ART" -D "$TMP" 2>/dev/null; then
  # Fallback: find a successful CI host run on that commit
  RUN_ID="$(gh run list -R "$REPO" --commit "$SHA" --workflow "CI host" --json databaseId,conclusion \
    --jq '[.[]|select(.conclusion=="success")][0].databaseId' 2>/dev/null || true)"
  [[ -n "$RUN_ID" && "$RUN_ID" != "null" ]] || {
    echo "[qtclient-ccache] no successful CI host run for $SHA" >&2
    exit 1
  }
  gh run download -R "$REPO" "$RUN_ID" -n "$ART" -D "$TMP"
fi

ARCHIVE="$(find "$TMP" -name 'ccache-*.tar.zst' | head -1)"
[[ -n "$ARCHIVE" ]] || { echo "[qtclient-ccache] archive missing in artifact" >&2; exit 1; }

mkdir -p "$OUT_DIR"
tar --zstd -xf "$ARCHIVE" -C "$OUT_DIR"
echo "[qtclient-ccache] restored → $OUT_DIR"
echo "CCACHE_DIR=$OUT_DIR"
