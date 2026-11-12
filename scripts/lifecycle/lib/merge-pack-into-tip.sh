#!/usr/bin/env bash
# Merge a product pack into the qtclient tip so ModLoader finds mods/<id>
# next to the Play binary.
#
# --tip is the applicationDirPath (desktop) or the directory that contains
# tibia.wasm (wasm). Destination: <tip>/mods/<pack-id>
# Keeps default packs (otclient, launcher clientmods/minimap); drops myaac
# (product urls replace it) and any other non-default siblings.
#
# Usage:
#   merge-pack-into-tip.sh --pack <mods/<id> tree> --tip <applicationDirPath>
set -euo pipefail

PACK=""
TIP=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --pack) PACK="${2:?}"; shift 2 ;;
    --tip) TIP="${2:?}"; shift 2 ;;
    --platform) shift 2 ;; # accepted for callers; dest layout is tip/mods/<id>
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done
[[ -n "$PACK" && -n "$TIP" ]] || {
  echo "usage: $0 --pack <dir> --tip <applicationDirPath>" >&2
  exit 1
}
test -f "$PACK/mod.json" || { echo "missing $PACK/mod.json" >&2; exit 1; }
test -d "$TIP" || { echo "missing tip dir $TIP" >&2; exit 1; }

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"

# Engine root → Contents/MacOS so ModLoader sees mods next to the Play binary.
# Callers often pass the qtclient tip root (…/engine) that contains *.app.
macos="$(find "$TIP" -maxdepth 3 -path '*/Contents/MacOS' -type d 2>/dev/null | head -1 || true)"
if [[ -n "$macos" && "$TIP" != "$macos" ]]; then
  echo "[merge-pack] tip → $macos (Contents/MacOS)"
  TIP="$macos"
fi

pack_id="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("id") or "")' "$PACK/mod.json")"
[[ -n "$pack_id" ]] || pack_id="$(basename "$PACK")"

mods_root="$TIP/mods"
# Keep default client + launcher packs; drop myaac (product urls) and unknowns.
keep="otclient|clientmods|minimap|${pack_id}"
if [[ -d "$mods_root" ]]; then
  for sibling in "$mods_root"/*; do
    [[ -e "$sibling" ]] || continue
    base="$(basename "$sibling")"
    [[ "$base" == *.files ]] && continue
    if [[ ! "$base" =~ ^($keep)$ ]]; then
      rm -rf "$sibling"
    fi
  done
fi

dest="$mods_root/$pack_id"
rm -rf "$dest"
mkdir -p "$mods_root"
cp -a "$PACK" "$dest"
test -f "$dest/mod.json"
# Runtime MODULE packs ship native/mod.* (host-pushed C ABI). Drop only stale lib/.
rm -rf "$dest/lib"
# Content fingerprint so /play OPFS restages when tip pack bytes change without a
# wasm stamp bump (brand chrome, QML overrides, …).
python3 - "$dest" <<'PY'
import hashlib, os, sys
from pathlib import Path
root = Path(sys.argv[1])
h = hashlib.sha256()
for path in sorted(root.rglob("*")):
    if not path.is_file():
        continue
    rel = path.relative_to(root).as_posix()
    if rel in {".pack.stamp", ".files.manifest"}:
        continue
    h.update(rel.encode())
    h.update(b"\0")
    h.update(path.read_bytes())
    h.update(b"\0")
(root / ".pack.stamp").write_text(h.hexdigest() + "\n")
PY
(
  cd "$dest"
  find . -type f | sed 's|^\./||' | sort
) >"${dest}.files"
# Manifest lives beside the tree as mods/<id>.files (${dest}.files).
echo "[merge-pack] ${PACK} → ${dest} ($(wc -l < "${dest}.files" | tr -d ' ') files)"

# Default launcher channel gates (clientmods + minimap).
bash "$ROOT/scripts/lifecycle/lib/stage-default-launcher-mods.sh" --tip "$TIP"
