#!/usr/bin/env bash
# Merge a built IdleOT pack tree into a Soft tip / engine layout so ModLoader
# finds mods/idleot next to the Play binary (CIP tip sync — no separate mods protocol).
#
# Usage:
#   merge-pack-into-tip.sh --pack <mods/idleot tree> --tip <engine tip root> [--platform linux-x64|windows-x64|macos-arm64|wasm]
set -euo pipefail

PACK=""
TIP=""
PLATFORM="linux-x64"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --pack) PACK="${2:?}"; shift 2 ;;
    --tip) TIP="${2:?}"; shift 2 ;;
    --platform) PLATFORM="${2:?}"; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done
[[ -n "$PACK" && -n "$TIP" ]] || {
  echo "usage: $0 --pack <dir> --tip <dir> [--platform …]" >&2
  exit 1
}
test -f "$PACK/mod.json" || { echo "missing $PACK/mod.json" >&2; exit 1; }

dest=""
case "$PLATFORM" in
  macos-arm64)
    # Prefer Contents/MacOS next to client (applicationDirPath()/mods).
    app="$(find "$TIP" -maxdepth 2 -name '*.app' -type d | head -1 || true)"
    if [[ -n "$app" ]]; then
      dest="$app/Contents/MacOS/mods/idleot"
    else
      dest="$TIP/mods/idleot"
    fi
    ;;
  wasm)
    # pack-tibia-wasm lays out client/<rev>/wasm/ — place mods beside tibia.wasm.
    wasm_dir="$(find "$TIP" -type f -name 'tibia.wasm' 2>/dev/null | head -1 | xargs -I{} dirname {} || true)"
    if [[ -z "$wasm_dir" ]]; then
      wasm_dir="$TIP"
    fi
    dest="$wasm_dir/mods/idleot"
    ;;
  *)
    dest="$TIP/mods/idleot"
    # Linux/Windows engine dir may be the tip root itself.
    if [[ -f "$TIP/client" || -f "$TIP/client.exe" ]]; then
      dest="$TIP/mods/idleot"
    elif [[ -d "$TIP/engine" ]]; then
      # pack-host-desktop layout
      eng="$(find "$TIP/engine" -maxdepth 2 \( -name client -o -name 'client.exe' \) -type f | head -1 | xargs -I{} dirname {} || true)"
      [[ -n "$eng" ]] && dest="$eng/mods/idleot"
    fi
    ;;
esac

rm -rf "$dest"
mkdir -p "$(dirname "$dest")"
cp -a "$PACK" "$dest"
test -f "$dest/mod.json"
# Manifest for portal /play OPFS staging (one path per line, relative to pack root).
(
  cd "$dest"
  find . -type f | sed 's|^\./||' | sort
) >"${dest}.files"
# Also keep a copy next to the pack for tip-relative fetch: mods/idleot.files
cp -f "${dest}.files" "$(dirname "$dest")/idleot.files"
echo "[merge-pack] ${PACK} → ${dest} ($(wc -l < "${dest}.files" | tr -d ' ') files)"
