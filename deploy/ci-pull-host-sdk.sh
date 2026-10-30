#!/usr/bin/env bash
# Wait for Soft host-sdk OCI tip and unpack into DEST.
#   deploy/ci-pull-host-sdk.sh <oras-ref> <dest-dir>
set -euo pipefail

REF="${1:-}"
DEST="${2:-}"
[[ -n "$REF" && -n "$DEST" ]] || {
  echo "usage: $0 <oras-ref> <dest-dir>" >&2
  exit 2
}
command -v oras >/dev/null || { echo "oras required" >&2; exit 1; }

mkdir -p "$DEST"
for i in $(seq 1 80); do
  if (cd "$DEST" && oras pull "$REF"); then
    break
  fi
  echo "host-sdk not ready ($i/80): $REF"
  sleep 90
  rm -rf "${DEST:?}"/* || true
done
cd "$DEST"
layer="$(find . -type f \( -name '*.tgz' -o -name '*.tar.gz' -o -name 'host-sdk.tgz' \) | head -1 || true)"
[[ -z "$layer" ]] && layer="$(find . -type f | head -1 || true)"
[[ -n "$layer" ]] && tar -xzf "$layer" 2>/dev/null || true
test -f cmake/QtClientMod.cmake -o -f host-sdk.json
echo "[host-sdk] ready in $DEST"
