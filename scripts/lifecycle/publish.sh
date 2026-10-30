#!/usr/bin/env bash
# qtclient publish — ORAS push a packaged dir to ghcr.io/<owner>/client-<kind>:<sha>
#
# Env:
#   PACKAGE_KIND=sdk|desktop|wasm
#   PACKAGE_OUT=<dir>           packed tree (required)
#   PLATFORM=…                  required when KIND=desktop
#   REGISTRY=ghcr.io/<owner>    default ghcr.io/qtclient
#   QTCLIENT_HOST_BUILD_ID / SOURCE_COMMIT / GITHUB_SHA
#   ORAS_USER / ORAS_PASSWORD or GITHUB_TOKEN + GITHUB_ACTOR (login)
#   PUBLISH_LOGIN=1             login before push (default 1 when token present)
set -euo pipefail
OTC="$(cd "$(dirname "$0")/../.." && pwd)"
KIND="${PACKAGE_KIND:-sdk}"
OUT="${PACKAGE_OUT:?PACKAGE_OUT required}"
HOST_ID="${QTCLIENT_HOST_BUILD_ID:-${SOURCE_COMMIT:-${GITHUB_SHA:-}}}"
if [[ -z "$HOST_ID" ]]; then
  HOST_ID="$(git -C "$OTC" rev-parse HEAD 2>/dev/null || echo unknown)"
fi
OWNER="${GITHUB_REPOSITORY_OWNER:-qtclient}"
REGISTRY="${REGISTRY:-ghcr.io/$OWNER}"

case "$KIND" in
  sdk) name="client-sdk"; tarball="client-sdk.tgz" ;;
  wasm) name="client-wasm"; tarball="client-wasm.tgz" ;;
  desktop)
    PLATFORM="${PLATFORM:?PLATFORM required for desktop publish}"
    name="client-$PLATFORM"
    tarball="client.tgz"
    ;;
  *)
    echo "[qtclient] publish: unknown PACKAGE_KIND=$KIND" >&2
    exit 1
    ;;
esac

command -v oras >/dev/null || { echo "oras required" >&2; exit 1; }
[[ -d "$OUT" ]] || { echo "[qtclient] publish: missing $OUT" >&2; exit 1; }

user="${ORAS_USER:-${GITHUB_ACTOR:-}}"
pass="${ORAS_PASSWORD:-${GITHUB_TOKEN:-}}"
if [[ "${PUBLISH_LOGIN:-}" != "0" && -n "$user" && -n "$pass" ]]; then
  echo "$pass" | oras login ghcr.io -u "$user" --password-stdin
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
tar -czf "$work/$tarball" -C "$OUT" .
ref="$REGISTRY/$name:$HOST_ID"
oras push "$ref" "$work/$tarball:application/vnd.oci.image.layer.v1.tar+gzip"
oras tag "$ref" "$REGISTRY/$name:sha-$HOST_ID" || true
echo "PUBLISHED=$ref"
