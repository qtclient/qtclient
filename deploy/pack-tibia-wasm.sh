#!/usr/bin/env bash
# Lay out the tibia WASM slice of the CDN bundle:
#   <dest>/client/<rev>/wasm/tibia.{html,js,wasm}[.gz|.br] qtloader.js qtlogo.svg …
#   <dest>/.qtclient/v3/content-web.json   minimal list (QML is compiled in; no Lua tree)
#   <dest>/.qtclient/wasm.json
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
mkdir -p "$OUT" "${DEST}/.qtclient/v3" "${DEST}/content"

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
(dest / ".qtclient/v3/content-web.json").write_text(json.dumps(doc, indent=1) + "\n")
print(f"[pack-tibia-wasm] content-web: {len(doc['files'])} files (tibia; QML in binary)")
PY

cat >"${DEST}/.qtclient/wasm.json" <<EOF
{
  "revision": "${REV}",
  "productSha": "${ENGINE_SHA}",
  "engine": "tibia",
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF

echo "[pack-tibia-wasm] revision=${REV} engine=${ENGINE_SHA} → ${OUT}"
