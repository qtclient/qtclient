#!/usr/bin/env bash
# Stage a stock Soft desktop host tip (no IdleOT pack) for GHCR / Release assemble.
# Usage: pack-host-desktop.sh --platform linux-x64|windows-x64|macos-arm64 \
#          --bin <build-bin-dir> --out <dest-dir>
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="$(cd "$HERE/.." && pwd)"
PLATFORM=""
BIN=""
OUT=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --platform) PLATFORM="${2:?}"; shift 2 ;;
    --bin) BIN="${2:?}"; shift 2 ;;
    --out) OUT="${2:?}"; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done
[[ -n "$PLATFORM" && -n "$BIN" && -n "$OUT" ]] || {
  echo "usage: $0 --platform <plat> --bin <dir> --out <dir>" >&2
  exit 1
}

HOST_ID="${QTCLIENT_HOST_BUILD_ID:-${SOURCE_COMMIT:-${GITHUB_SHA:-$(git -C "$OTC" rev-parse HEAD)}}}"
export QTCLIENT_HOST_BUILD_ID="$HOST_ID"
export SOURCE_COMMIT="${SOURCE_COMMIT:-$HOST_ID}"
# Stock Soft branding for host artifacts (IdleOT brand happens at Release assemble).
export QTCLIENT_APP_NAME="${QTCLIENT_APP_NAME:-qtclient}"
export QTCLIENT_BUNDLE_ID="${QTCLIENT_BUNDLE_ID:-com.qtclient.client}"
export QTCLIENT_STORE_META="${QTCLIENT_STORE_META:-.qtclient}"

rm -rf "$OUT"
mkdir -p "$OUT/bin" "$OUT/engine"

ENGINE_BUNDLE="$OUT/engine"
python3 "$OTC/tools/publish/package_desktop.py" \
  --platform "$PLATFORM" \
  --bin "$BIN" \
  --engine-bundle "$ENGINE_BUNDLE"

# Keep Soft binaries for Release assemble / package_launcher.
find_bin() {
  local name="$1"
  local cand
  for cand in \
    "$BIN/$name" "$BIN/$name.exe" \
    "$BIN/../launcher/$name" \
    "$BIN/launcher/$name"
  do
    if [[ -f "$cand" ]]; then echo "$cand"; return 0; fi
  done
  find "$(dirname "$BIN")" -type f \( -name "$name" -o -name "$name.exe" \) 2>/dev/null | head -1 || true
}
for name in client launcher; do
  b="$(find_bin "$name" || true)"
  if [[ -n "$b" && -f "$b" ]]; then
    cp -a "$b" "$OUT/bin/$(basename "$b")"
  fi
done

cat >"$OUT/host.json" <<EOF
{
  "hostBuildId": "${HOST_ID}",
  "platform": "${PLATFORM}",
  "schema": 1,
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF

echo "[pack-host-desktop] ${PLATFORM} hostBuildId=${HOST_ID} → ${OUT}"
