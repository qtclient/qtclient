#!/usr/bin/env bash
set -euo pipefail
QTCLIENT="$(cd "$(dirname "$0")/../../.." && pwd)"
OUT="${PACKAGE_OUT:?PACKAGE_OUT required}"
HOST_ID="${QTCLIENT_HOST_BUILD_ID:-${SOURCE_COMMIT:-${GITHUB_SHA:-}}}"
if [[ -z "$HOST_ID" ]]; then
  HOST_ID="$(git -C "$QTCLIENT" rev-parse HEAD 2>/dev/null || echo unknown)"
fi
OWNER="${GITHUB_REPOSITORY_OWNER:-qtclient}"
REGISTRY="${REGISTRY:-ghcr.io/$OWNER}"
# Desktop SDK: client-sdk; wasm SDK: client-sdk-wasm (PACKAGE_NAME or COMP=wasm).
NAME="${SDK_PACKAGE_NAME:-client-sdk}"
case "${COMP:-}" in browser|wasm) NAME="${SDK_PACKAGE_NAME:-client-sdk-wasm}" ;; esac
TARBALL="client-sdk.tgz"
command -v oras >/dev/null || { echo "oras required" >&2; exit 1; }
[[ -d "$OUT" ]] || { echo "[qtclient] publish: missing $OUT" >&2; exit 1; }
user="${ORAS_USER:-${GITHUB_ACTOR:-}}"
pass="${ORAS_PASSWORD:-${GITHUB_TOKEN:-}}"
if [[ "${PUBLISH_LOGIN:-}" != "0" && -n "$user" && -n "$pass" ]]; then
  echo "$pass" | oras login ghcr.io -u "$user" --password-stdin
fi
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
tar -czf "$work/$TARBALL" -C "$OUT" .
ref="$REGISTRY/$NAME:$HOST_ID"
( cd "$work" && oras push "$ref" "./$TARBALL:application/vnd.oci.image.layer.v1.tar+gzip" )
oras tag "$ref" "$REGISTRY/$NAME:sha-$HOST_ID" || true
echo "PUBLISHED=$ref"
