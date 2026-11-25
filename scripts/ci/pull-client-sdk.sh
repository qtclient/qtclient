#!/usr/bin/env bash
# Wait for qtclient client-sdk OCI tip and unpack into DEST.
#   scripts/ci/pull-client-sdk.sh <oras-ref> <dest-dir>
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
  echo "client-sdk not ready ($i/80): $REF"
  sleep 90
  rm -rf "${DEST:?}"/* || true
done
cd "$DEST"
layer="$(find . -type f \( -name '*.tgz' -o -name '*.tar.gz' -o -name 'client-sdk.tgz' -o -name 'host-sdk.tgz' \) | head -1 || true)"
[[ -z "$layer" ]] && layer="$(find . -type f | head -1 || true)"
[[ -n "$layer" ]] && tar -xzf "$layer" 2>/dev/null || true
# QtClientSdk layout (host API v9): abi headers + lib/cmake/QtClientSdk; no cipgen.
test -f lib/cmake/QtClientSdk/QtClientSdkConfig.cmake
test -f lib/cmake/QtClientSdk/QtClientMod.cmake
test -f include/qtclient/abi/host_api_v1.h
test -f share/qtclient-sdk/sdk.json -o -f client-sdk.json
echo "[client-sdk] ready in $DEST (find_package(QtClientSdk) with CMAKE_PREFIX_PATH=$DEST)"
