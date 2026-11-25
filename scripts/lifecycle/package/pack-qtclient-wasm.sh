#!/usr/bin/env bash
# Lay out the tibia WASM slice of the CDN bundle:
#   <dest>/client/<rev>/wasm/tibia.{html,js,wasm}[.gz|.br] qtloader.js qtlogo.svg …
#   <dest>/<meta>/v3/content-web.json    minimal list (QML is compiled in; no Lua tree)
#   <dest>/<meta>/wasm.json              slice metadata (the bundle assembler reads it)
# <meta> is QTCLIENT_STORE_META (default .qtclient, as package_desktop.py).
# A pack (QTCLIENT_PACK_DIR, else mods/<id> next to the wasm) is baked into the
# tip; its launcher tree comes from QTCLIENT_PACK/launcher.
# Usage: pack-qtclient-wasm.sh <src-dir> <dest-dir>
set -euo pipefail

SRC="${1:?src dir}"
DEST="${2:?dest dir}"
ENGINE_SHA="${TIBIA_ENGINE_SHA:-unknown}"

for f in tibia.js tibia.wasm tibia.html; do
  if [[ ! -f "${SRC}/${f}" ]]; then
    echo "[pack-qtclient-wasm] missing ${SRC}/${f}" >&2
    exit 1
  fi
done

sha256() { if command -v sha256sum >/dev/null; then sha256sum; else shasum -a 256; fi; }
REV="$(cat "${SRC}/tibia.js" "${SRC}/tibia.wasm" | sha256 | awk '{print substr($1,1,16)}')"
if [[ -n "${QTCLIENT_CLIENT_REVISION:-}" ]]; then
  REV="${QTCLIENT_CLIENT_REVISION}"
fi

META="${QTCLIENT_STORE_META:-.qtclient}"
OUT="${DEST}/client/${REV}/wasm"
mkdir -p "$OUT" "${DEST}/${META}/v3" "${DEST}/content"

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
  # tibia.data is a literal (not a glob): nullglob keeps it when the build
  # preloads nothing, and a failing copy_file here would end the script
  # silently under set -e.
  [[ -e "$f" ]] || continue
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
python3 - "$DEST" "$REV" "$META" <<'PY'
import json, sys
from pathlib import Path
dest, rev, meta = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
doc = {"schema": 3, "revision": rev, "engine": rev, "files": {}}
(dest / meta / "v3" / "content-web.json").write_text(json.dumps(doc, indent=1) + "\n")
print(f"[pack-qtclient-wasm] content-web: {len(doc['files'])} files (tibia; QML in binary)")
PY

cat >"${DEST}/${META}/wasm.json" <<EOF
{
  "revision": "${REV}",
  "productSha": "${ENGINE_SHA}",
  "engine": "tibia",
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF

# Optional pack: bake into tip next to tibia.wasm for /play OPFS.
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
      [[ "$base" == portal ]] && continue
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
  echo "[pack-qtclient-wasm] merged pack → ${OUT}/mods/${pack_id} (brand=$([[ -f ${OUT}/mods/${pack_id}/brand.json ]] && echo yes || echo no))"
fi
# The pack's launcher tree (brand, channels, chrome) even when PACK_SRC was a non-branded tree.
PACK_ROOT="${QTCLIENT_PACK:-}"
if [[ -n "$PACK_ROOT" && -f "$PACK_ROOT/launcher/brand.json" && -d "${OUT}/mods" ]]; then
  bash "$(cd "$(dirname "$0")/.." && pwd)/lib/merge-launcher-brand.sh" \
    --pack "$PACK_ROOT/launcher" --tip "$OUT" || true
fi
# Content fingerprint so /play OPFS restages when tip pack bytes change without a
# wasm stamp bump (brand chrome, QML overrides, …). Same contract as merge-pack-into-tip.
if [[ -d "${OUT}/mods" ]]; then
  for mid in "${OUT}/mods"/*; do
    [[ -d "$mid" && -f "$mid/mod.json" ]] || continue
    id="$(basename "$mid")"
    python3 - "$mid" <<'PY'
import hashlib, sys
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
      cd "$mid"
      find . -type f | sed 's|^\./||' | sort
    ) >"${OUT}/mods/${id}.files"
    echo "[pack-qtclient-wasm] stamped mods/${id} ($(wc -l < "${OUT}/mods/${id}.files" | tr -d ' ') files)"
  done
fi

echo "[pack-qtclient-wasm] revision=${REV} engine=${ENGINE_SHA} → ${OUT}"
