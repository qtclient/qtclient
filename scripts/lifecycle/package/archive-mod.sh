#!/usr/bin/env bash
# Phase 3: stage a pack archive (STATIC .a + FS chrome) for tip link via
# QTCLIENT_LINKED_PACK_LIBS=<id>=/path/to/libqtclient_mod_<id>.a
#
# Usage:
#   archive-mod.sh --id idleot --build-dir apps/qtclient/build/<preset> --out DIR
#   archive-mod.sh --id idleot --archive /path/to/libqtclient_mod_idleot.a \
#                  --fs /path/to/mods/idleot --out DIR
set -euo pipefail

ID=""
BUILD_DIR=""
ARCHIVE=""
FS=""
OUT=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --id) ID="${2:?}"; shift 2 ;;
    --build-dir) BUILD_DIR="${2:?}"; shift 2 ;;
    --archive) ARCHIVE="${2:?}"; shift 2 ;;
    --fs) FS="${2:?}"; shift 2 ;;
    --out) OUT="${2:?}"; shift 2 ;;
    -h|--help)
      sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done

[[ -n "$ID" && -n "$OUT" ]] || {
  echo "archive-mod.sh: --id and --out required" >&2
  exit 1
}

if [[ -z "$ARCHIVE" && -n "$BUILD_DIR" ]]; then
  for cand in \
    "$BUILD_DIR/mods/$ID/lib/libqtclient_mod_${ID}.a" \
    "$BUILD_DIR/mods/$ID/lib/qtclient_mod_${ID}.a" \
    "$BUILD_DIR/mods/$ID/lib/libqtclient_mod_${ID}.lib" \
    "$BUILD_DIR/lib/libqtclient_mod_${ID}.a"
  do
    if [[ -f "$cand" ]]; then
      ARCHIVE="$cand"
      break
    fi
  done
fi
if [[ -z "$FS" && -n "$BUILD_DIR" ]]; then
  for cand in \
    "$BUILD_DIR/mods/$ID" \
    "$BUILD_DIR/bin/mods/$ID"
  do
    if [[ -f "$cand/mod.json" ]]; then
      FS="$cand"
      break
    fi
  done
fi

[[ -n "$ARCHIVE" && -f "$ARCHIVE" ]] || {
  echo "archive-mod.sh: STATIC archive missing for id=$ID (pass --archive or --build-dir)" >&2
  exit 1
}
[[ -n "$FS" && -f "$FS/mod.json" ]] || {
  echo "archive-mod.sh: FS pack missing mod.json for id=$ID (pass --fs or --build-dir)" >&2
  exit 1
}

rm -rf "$OUT"
mkdir -p "$OUT/lib" "$OUT/mods/$ID"
cp -a "$ARCHIVE" "$OUT/lib/"
# FS chrome without nested lib/native
rsync -a --exclude lib --exclude native "$FS/" "$OUT/mods/$ID/"
# Convenience pointer for QTCLIENT_LINKED_PACK_LIBS
ARCH_NAME="$(basename "$ARCHIVE")"
cat >"$OUT/pack-archive.json" <<EOF
{
  "id": "$ID",
  "archive": "lib/$ARCH_NAME",
  "modDir": "mods/$ID",
  "linkFlag": "${ID}=$OUT/lib/$ARCH_NAME"
}
EOF
echo "[archive-mod] $ID → $OUT (lib/$ARCH_NAME + mods/$ID)"
