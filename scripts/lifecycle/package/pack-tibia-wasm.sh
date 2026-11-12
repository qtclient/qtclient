#!/usr/bin/env bash
# Lay out the tibia WASM slice of the CDN bundle:
#   <dest>/client/<rev>/wasm/tibia.{html,js,wasm}[.gz|.br] qtloader.js qtlogo.svg …
#   <dest>/.idleot/v3/content-web.json   minimal list (QML is compiled in; no Lua tree)
#   <dest>/.idleot/wasm.json             (assemble.py reads .idleot/)
# Usage: pack-tibia-wasm.sh <src-dir> <dest-dir>
set -euo pipefail

SRC="${1:?src dir}"
DEST="${2:?dest dir}"
ENGINE_SHA="${TIBIA_ENGINE_SHA:-unknown}"

for f in tibia.js tibia.wasm tibia.html; do
  if [[ ! -f "${SRC}/${f}" ]]; then
    echo "[pack-tibia-wasm] missing ${SRC}/${f}" >&2
    exit 1
  fi
done

sha256() { if command -v sha256sum >/dev/null; then sha256sum; else shasum -a 256; fi; }
REV="$(cat "${SRC}/tibia.js" "${SRC}/tibia.wasm" | sha256 | awk '{print substr($1,1,16)}')"
if [[ -n "${QTCLIENT_CLIENT_REVISION:-}" ]]; then
  REV="${QTCLIENT_CLIENT_REVISION}"
fi

OUT="${DEST}/client/${REV}/wasm"
mkdir -p "$OUT" "${DEST}/.idleot/v3" "${DEST}/content"

# Flattened build dirs may use symlinks into client/tibia/ — dereference.
copy_file() {
  local src="$1" dest="$2"
  [[ -e "$src" ]] || return 1
  rm -f "$dest"
  cp -L "$src" "$dest"
}

for f in tibia.html tibia.js tibia.wasm qtloader.js qtlogo.svg launcher.html launcher.js launcher.wasm; do
  [[ -e "${SRC}/${f}" ]] || continue
  copy_file "${SRC}/${f}" "${OUT}/${f}"
done
# Emscripten/Qt sidecar data (preload) if present.
shopt -s nullglob
for f in "${SRC}"/tibia.data "${SRC}"/*.wasm "${SRC}"/qtloader.js; do
  base="$(basename "$f")"
  [[ -e "${OUT}/${base}" ]] || copy_file "$f" "${OUT}/${base}"
done
shopt -u nullglob

for f in tibia.js tibia.wasm; do
  [[ -f "${OUT}/${f}" ]] || continue
  for ext in gz br; do
    if [[ -e "${SRC}/${f}.${ext}" ]]; then
      copy_file "${SRC}/${f}.${ext}" "${OUT}/${f}.${ext}"
    fi
  done
  if [[ ! -f "${OUT}/${f}.gz" ]]; then
    gzip -6 -n -c "${OUT}/${f}" > "${OUT}/${f}.gz"
  fi
done

# Minimal runtime list so the launcher sync path still has a content-web.json.
python3 - "$DEST" "$REV" <<'PY'
import json, sys
from pathlib import Path
dest, rev = Path(sys.argv[1]), sys.argv[2]
doc = {"schema": 3, "revision": rev, "engine": rev, "files": {}}
(dest / ".idleot/v3/content-web.json").write_text(json.dumps(doc, indent=1) + "\n")
print(f"[pack-tibia-wasm] content-web: {len(doc['files'])} files (tibia; QML in binary)")
PY

cat >"${DEST}/.idleot/wasm.json" <<EOF
{
  "revision": "${REV}",
  "productSha": "${ENGINE_SHA}",
  "engine": "tibia",
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF

# Optional product pack: bake into tip next to tibia.wasm for /play OPFS.
# Prefer QTCLIENT_PACK_DIR; else mods/<id> next to the wasm (prefer bin/ with brand.json).
PACK_SRC="${QTCLIENT_PACK_DIR:-}"
if [[ -z "$PACK_SRC" ]]; then
  for mods_root in "${SRC}/mods" "${SRC}/bin/mods" "$(dirname "$SRC")/mods" "$(dirname "$SRC")/bin/mods"; do
    [[ -d "$mods_root" ]] || continue
    branded=""
    first=""
    for cand in "$mods_root"/*; do
      [[ -d "$cand" && -f "$cand/mod.json" ]] || continue
      base="$(basename "$cand")"
      [[ "$base" == otclient || "$base" == myaac || "$base" == clientmods || "$base" == minimap ]] && continue
      [[ -z "$first" ]] && first="$cand"
      if [[ -f "$cand/brand.json" ]]; then
        branded="$cand"
        break
      fi
    done
    PACK_SRC="${branded:-$first}"
    [[ -n "$PACK_SRC" ]] && break
  done
fi
if [[ -n "$PACK_SRC" && -f "${PACK_SRC}/mod.json" ]]; then
  pack_id="$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1])).get("id") or "")' "${PACK_SRC}/mod.json")"
  [[ -n "$pack_id" ]] || pack_id="$(basename "$PACK_SRC")"
  rm -rf "${OUT}/mods/${pack_id}"
  mkdir -p "${OUT}/mods"
  cp -a "$PACK_SRC" "${OUT}/mods/${pack_id}"
  # Drop linked STATIC archives from the FS tip (absorb path loads from the wasm).
  rm -rf "${OUT}/mods/${pack_id}/lib"
  (
    cd "${OUT}/mods/${pack_id}"
    find . -type f | sed 's|^\./||' | sort
  ) >"${OUT}/mods/${pack_id}.files"
  echo "[pack-tibia-wasm] merged pack → ${OUT}/mods/${pack_id} ($(wc -l < "${OUT}/mods/${pack_id}.files" | tr -d ' ') files, brand=$([[ -f ${OUT}/mods/${pack_id}/brand.json ]] && echo yes || echo no))"
fi
# Ensure launcher chrome lands even when PACK_SRC was a non-branded tree.
PACK_ROOT="${QTCLIENT_PACK:-}"
if [[ -z "$PACK_ROOT" ]]; then
  _umbrella="$(cd "$(dirname "$0")/../../../.." && pwd)"
  [[ -f "$_umbrella/apps/idleot-client/launcher/brand.json" ]] && PACK_ROOT="$_umbrella/apps/idleot-client"
fi
if [[ -n "$PACK_ROOT" && -f "$PACK_ROOT/launcher/brand.json" && -d "${OUT}/mods" ]]; then
  bash "$(cd "$(dirname "$0")/.." && pwd)/lib/merge-launcher-brand.sh" \
    --pack "$PACK_ROOT/launcher" --tip "$OUT" || true
  for mid in "${OUT}/mods"/*; do
    [[ -d "$mid" && -f "$mid/mod.json" ]] || continue
    id="$(basename "$mid")"
    (
      cd "$mid"
      find . -type f | sed 's|^\./||' | sort
    ) >"${OUT}/mods/${id}.files"
  done
fi

echo "[pack-tibia-wasm] revision=${REV} engine=${ENGINE_SHA} → ${OUT}"
