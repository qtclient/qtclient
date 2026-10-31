#!/usr/bin/env bash
# Export a clean public tree and push a fresh history to qtclient/qtclient.
#
#   scripts/push-qtclient.sh [--dry-run] [--dest DIR]
#
# qt5-cip is recorded as a gitlink only (update=none) — no full checkout on
# the pusher machine (multi-GB). Consumers / make qt init it when needed.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${QTCLIENT_DEST:-/tmp/qtclient-public}"
REMOTE="${QTCLIENT_REMOTE:-git@github.com:qtclient/qtclient.git}"
DRY=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --dry-run) DRY=1; shift ;;
    --dest) DEST="$2"; shift 2 ;;
    --remote) REMOTE="$2"; shift 2 ;;
    *) echo "unknown: $1" >&2; exit 2 ;;
  esac
done

rm -rf "$DEST"
bash "$ROOT/scripts/export-public-client.sh" "$DEST"

cd "$DEST"
rm -rf .git
find . -name '.git' -print -exec rm -rf {} + 2>/dev/null || true
git init -b main

LC_SHA="$(awk '/^Qt5LegacyControls/{print $2}' third_party/SUBMODULE_PINS.txt 2>/dev/null || true)"
QT_SHA="$(awk '/^qt5-cip/{print $2}' third_party/SUBMODULE_PINS.txt 2>/dev/null || true)"
[[ -n "$LC_SHA" ]] || LC_SHA=989ad584cf0abb89295d0d292e4ca4ab78734d0d
[[ -n "$QT_SHA" ]] || { echo "missing qt5-cip pin" >&2; exit 1; }

rm -rf third_party/Qt5LegacyControls third_party/qt5-cip 2>/dev/null || true
mkdir -p third_party

# Small submodule: full checkout OK
git submodule add -b main https://github.com/CipSoft-Tibia/Qt5LegacyControls.git third_party/Qt5LegacyControls
git -C third_party/Qt5LegacyControls checkout "$LC_SHA"
git add third_party/Qt5LegacyControls

# Large submodule: gitlink + .gitmodules only (no working tree)
{
  echo '[submodule "third_party/Qt5LegacyControls"]'
  echo $'\tpath = third_party/Qt5LegacyControls'
  echo $'\turl = https://github.com/CipSoft-Tibia/Qt5LegacyControls.git'
  echo $'\tbranch = main'
  echo '[submodule "third_party/qt5-cip"]'
  echo $'\tpath = third_party/qt5-cip'
  echo $'\turl = https://github.com/CipSoft-Tibia/Qt5.git'
  echo $'\tbranch = master'
  echo $'\tupdate = none'
  echo $'\tshallow = true'
} > .gitmodules

git add .gitmodules third_party/SUBMODULE_PINS.txt
git add -A
# Re-assert qt5-cip gitlink after add -A (no working tree for the multi-GB module).
git rm -rf --cached third_party/qt5-cip 2>/dev/null || true
rm -rf third_party/qt5-cip
git update-index --add --cacheinfo "160000,${QT_SHA},third_party/qt5-cip"

# Brand gate before commit
if command -v rg >/dev/null; then
  if rg -n 'idleot::|#include\s*"idleot/' client/src 2>/dev/null; then
    echo "abort: idleot types in client/src" >&2
    exit 1
  fi
fi

git -c user.email="noreply@qtclient.dev" -c user.name="qtclient" commit -m "$(cat <<'EOF'
Initial public qtclient (otqt) tree.

CipSoft-compatible Qt/QML client with mods API, CipSoft Qt5 + LegacyControls
submodules, and stock CI / GHCR warm paths. No product packs or CIP assets.
EOF
)"

echo "Commit: $(git rev-parse --short HEAD)"
git -c user.email="noreply@qtclient.dev" -c user.name="qtclient" \
  submodule status || true

if [[ "$DRY" == "1" ]]; then
  echo "[dry-run] would push $DEST → $REMOTE"
  git log -1 --oneline
  exit 0
fi

git remote add origin "$REMOTE"
if [[ "${QTCLIENT_FORCE:-0}" == "1" ]]; then
  git push -u origin main --force
else
  git push -u origin main
fi

# Repo metadata (best-effort)
gh repo edit qtclient/qtclient \
  --description "CipSoft-compatible Qt/QML Tibia client (otqt)" \
  --homepage "https://github.com/qtclient/qtclient" \
  --add-topic tibia --add-topic qt --add-topic qml --add-topic cipsoft 2>/dev/null || true

echo "Pushed to $REMOTE"
