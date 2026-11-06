#!/usr/bin/env bash
# Export a public-facing qtclient tree (CIP host, no private product packs).
# Prefer a fresh git history when pushing.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${1:?usage: $0 <dest-dir>}"

mkdir -p "$DEST"
rsync -a --delete \
  --exclude '.git/' \
  --exclude 'build/' \
  --exclude 'build-*/' \
  --exclude 'bin/' \
  --exclude 'data/assets/' \
  --exclude 'data/things/' \
  --exclude 'data/sounds/' \
  --exclude 'client/mods/_*/' \
  --exclude '*.app/' \
  --exclude '**/node_modules/' \
  "$ROOT/" "$DEST/"

# Public gitignore: stock default packs only; ignore upstream when spinning out.
GITIGNORE="$DEST/.gitignore"
if [[ -f "$GITIGNORE" ]]; then
  # Drop any product-pack allowlist leftovers; keep stock myaac/otclient.
  sed -i.bak -E '/^!\/client\/mods\/[^_/]/d;/^!\/data\/sounds\//d' "$GITIGNORE" 2>/dev/null || true
  rm -f "$GITIGNORE.bak"
  if ! grep -q '^/upstream/' "$GITIGNORE" 2>/dev/null; then
    printf '\n# Public CIP-clone spin-out ignores vendored upstream.\n/upstream/\n' >>"$GITIGNORE"
  fi
fi

# Neutral brand tokens in exported tree.
find "$DEST" -type f \( -name '*.md' -o -name '*.sh' -o -name '*.py' -o -name '*.yml' \
  -o -name '*.yaml' -o -name '*.cmake' -o -name 'CMakeLists.txt' -o -name 'Makefile' \
  -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' -o -name '*.json' \) \
  -print0 2>/dev/null | while IFS= read -r -d '' f; do
  sed -i.bak \
    -e 's|ghcr\.io/otquartz/qt-cip|ghcr.io/qtclient/qt-cip|g' \
    -e 's/otquartz/qtclient/g' \
    "$f" 2>/dev/null || true
  rm -f "${f}.bak"
done

if [[ -f "$DEST/Makefile" ]]; then
  sed -i.bak 's/qtclient — .* tibia client/qtclient (otqt)/g;s/otquartz/qtclient/g' "$DEST/Makefile" || true
  rm -f "$DEST/Makefile.bak"
fi

echo "exported → $DEST"
