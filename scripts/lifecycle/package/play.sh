#!/usr/bin/env bash
# Emit a local CIP-shaped multi-client tip (worktrees + product packs).
# Shared assetsurl; package.json.version moves on code/pack-only changes.
#
# Usage:
#   play.sh [--current-only] [--cip-assets] [--pack DIR]…
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="$(cd "$HERE/../../.." && pwd)"
ROOT="$(cd "$OTC/../.." && pwd)"
OUT="${QTCLIENT_PLAY_TIP:-$ROOT/dist/play/tip}"
ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out) OUT="${2:?}"; shift 2 ;;
    --current-only|--cip-assets|--list) ARGS+=("$1"); shift ;;
    --pack) ARGS+=(--pack "${2:?}"); shift 2 ;;
    --cdn-url) ARGS+=(--cdn-url "${2:?}"); shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done

export DEV="${DEV:-1}"
echo "[play-pack] emit tip → $OUT"
if [[ ${#ARGS[@]} -gt 0 ]]; then
  python3 "$OTC/tools/publish/emit_play_tip.py" --out "$OUT" --umbrella "$ROOT" "${ARGS[@]}"
else
  python3 "$OTC/tools/publish/emit_play_tip.py" --out "$OUT" --umbrella "$ROOT"
fi
echo "[play-pack] QTCLIENT_URL=file://$OUT/tibiametadata.json"
