#!/usr/bin/env bash
# Stage a launcher brand pack next to qtclient launcher tip (mods/<id>/).
# qtclient binary stays CipSoft; IdleOT identity + art load at runtime unless --no-mods.
#
# Usage:
#   merge-launcher-brand.sh --pack <launcher/mods tree> --tip <applicationDirPath>
set -euo pipefail

PACK=""
TIP=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --pack) PACK="${2:?}"; shift 2 ;;
    --tip) TIP="${2:?}"; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done
[[ -n "$PACK" && -n "$TIP" ]] || {
  echo "usage: $0 --pack <dir> --tip <applicationDirPath>" >&2
  exit 1
}
test -f "$PACK/brand.json" || { echo "missing $PACK/brand.json" >&2; exit 1; }
test -d "$TIP" || { echo "missing tip dir $TIP" >&2; exit 1; }

id="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("id","idleot"))' "$PACK/brand.json")"
dest="$TIP/mods/$id"
rm -rf "$dest"
mkdir -p "$dest"
# brand + optional images/ (and leftover brand.cmake / mod.json are fine).
cp -a "$PACK/brand.json" "$dest/"
[[ -f "$PACK/mod.json" ]] && cp -a "$PACK/mod.json" "$dest/"
if [[ -d "$PACK/images" ]]; then
  cp -a "$PACK/images" "$dest/"
fi
echo "[merge-launcher-brand] ${PACK} → ${dest}"
