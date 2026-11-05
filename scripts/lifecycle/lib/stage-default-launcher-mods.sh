#!/usr/bin/env bash
# Stage default launcher mods (clientmods + minimap) into tip mods/.
# These are launcher.json-only packs (no client mod.json) that gate non-CIP
# download channels. Usage:
#   stage-default-launcher-mods.sh --tip <applicationDirPath>
set -euo pipefail

TIP=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --tip) TIP="${2:?}"; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done
[[ -n "$TIP" ]] || { echo "usage: $0 --tip <applicationDirPath>" >&2; exit 1; }

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
SRC="$ROOT/launcher/mods"
test -d "$TIP" || { echo "missing tip dir $TIP" >&2; exit 1; }

# Engine root → Contents/MacOS so launcher sees mods next to the binary.
macos="$(find "$TIP" -maxdepth 3 -path '*/Contents/MacOS' -type d 2>/dev/null | head -1 || true)"
if [[ -n "$macos" && "$TIP" != "$macos" ]]; then
  TIP="$macos"
fi

mods_root="$TIP/mods"
mkdir -p "$mods_root"
for id in clientmods minimap; do
  test -f "$SRC/$id/launcher.json" || { echo "missing $SRC/$id/launcher.json" >&2; exit 1; }
  dest="$mods_root/$id"
  rm -rf "$dest"
  mkdir -p "$dest"
  cp -a "$SRC/$id/launcher.json" "$dest/"
  echo "[stage-launcher-mods] $id → $dest"
done
