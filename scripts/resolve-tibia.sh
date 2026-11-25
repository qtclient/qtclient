#!/usr/bin/env bash
# Print the official Tibia install path, or exit 1 with how to override.
# Prefer $TIBIA; else probe CipSoft package locations per OS.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
QTCLIENT="$(cd "$HERE/.." && pwd)"

# Prefer in-tree / monorepo Python resolver.
for cand in \
  "$QTCLIENT/tools/tibia_paths.py" \
  "$QTCLIENT/../../tools/tibia_paths.py" \
  "$HERE/../../tools/tibia_paths.py"
do
  if [[ -f "$cand" ]]; then
    exec python3 "$cand" "$@"
  fi
done

echo "[tibia] tools/tibia_paths.py missing" >&2
exit 1
