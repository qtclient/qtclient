#!/usr/bin/env bash
# Stage a stock demo Release: launcher installer + client package tip +
# tibiametadata.json (CipSoft assetsurl). Login via mods/default (not release_env).
#
# Usage:
#   pack-demo-release.sh --platform linux-x64|windows-x64|macos-arm64 \
#     --bin <build-bin-dir> --out <dest-dir> [--release-base-url URL]
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="$(cd "$HERE/.." && pwd)"
PLATFORM=""
BIN=""
OUT=""
RELEASE_BASE="${QTCLIENT_DEMO_RELEASE_URL:-https://github.com/qtclient/qtclient/releases/latest/download}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --platform) PLATFORM="${2:?}"; shift 2 ;;
    --bin) BIN="${2:?}"; shift 2 ;;
    --out) OUT="${2:?}"; shift 2 ;;
    --release-base-url) RELEASE_BASE="${2:?}"; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done
[[ -n "$PLATFORM" && -n "$BIN" && -n "$OUT" ]] || {
  echo "usage: $0 --platform <plat> --bin <dir> --out <dir>" >&2
  exit 1
}

HOST_ID="${QTCLIENT_HOST_BUILD_ID:-${SOURCE_COMMIT:-${GITHUB_SHA:-$(git -C "$OTC" rev-parse HEAD 2>/dev/null || echo dev)}}}"
export QTCLIENT_HOST_BUILD_ID="$HOST_ID"
export SOURCE_COMMIT="${SOURCE_COMMIT:-$HOST_ID}"
export QTCLIENT_APP_NAME="${QTCLIENT_APP_NAME:-qtclient}"
export QTCLIENT_BUNDLE_ID="${QTCLIENT_BUNDLE_ID:-com.qtclient.client}"
export QTCLIENT_STORE_META="${QTCLIENT_STORE_META:-.qtclient}"
rm -rf "$OUT"
mkdir -p "$OUT/launcher" "$OUT/engine" "$OUT/tip"

echo "[pack-demo] staging launcher ($PLATFORM)…"
python3 "$OTC/tools/publish/package_launcher.py" \
  --platform "$PLATFORM" \
  --bin "$BIN" \
  --out "$OUT/launcher"

echo "[pack-demo] staging client engine ($PLATFORM)…"
ENGINE_BUNDLE="$OUT/engine"
python3 "$OTC/tools/publish/package_desktop.py" \
  --platform "$PLATFORM" \
  --bin "$BIN" \
  --engine-bundle "$ENGINE_BUNDLE"

TREE=""
case "$PLATFORM" in
  macos-arm64)
    TREE="$ENGINE_BUNDLE/${QTCLIENT_APP_NAME}.app"
    [[ -d "$TREE" ]] || TREE="$ENGINE_BUNDLE/client.app"
    ;;
  windows-x64|linux-x64)
    TREE="$ENGINE_BUNDLE"
    ;;
esac
[[ -d "$TREE" || -f "$TREE/client" || -f "$TREE/client.exe" ]] || {
  echo "[pack-demo] missing staged client tree under $ENGINE_BUNDLE" >&2
  ls -la "$ENGINE_BUNDLE" >&2 || true
  exit 1
}

# Ensure default (myAAC) pack is present for staging into the client tree.
[[ -f "$OTC/client/mods/default/mod.json" ]] || {
  echo "[pack-demo] missing $OTC/client/mods/default/mod.json" >&2
  exit 1
}

echo "[pack-demo] emit demo tip (mods/default + CipSoft assetsurl)…"
python3 "$OTC/tools/publish/emit_demo_tip.py" \
  --platform "$PLATFORM" \
  --tree "$TREE" \
  --out "$OUT/tip" \
  --revision "$HOST_ID" \
  --release-base-url "$RELEASE_BASE"

# Sidecar tip next to staged launcher for local double-click tests.
case "$PLATFORM" in
  macos-arm64)
    for app in "$OUT/launcher/installers/$PLATFORM"/*.app; do
      [[ -d "$app" ]] || continue
      cp -f "$OUT/tip/tibiametadata.json" "$app/Contents/Resources/tibiametadata.json"
      echo "[pack-demo] sidecar tip → $app/Contents/Resources/"
    done
    ;;
  *)
    if [[ -d "$OUT/launcher/installers/$PLATFORM" ]]; then
      cp -f "$OUT/tip/tibiametadata.json" "$OUT/launcher/installers/$PLATFORM/tibiametadata.json"
      echo "[pack-demo] sidecar tip → $OUT/launcher/installers/$PLATFORM/"
    fi
    ;;
esac

cat >"$OUT/demo.json" <<EOF
{
  "hostBuildId": "${HOST_ID}",
  "platform": "${PLATFORM}",
  "loginMod": "default",
  "schema": 1,
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF

echo "[pack-demo] ${PLATFORM} hostBuildId=${HOST_ID} → ${OUT}"
echo "[pack-demo] attach tip/*.json + launcher installers; do NOT attach CipSoft assets"
