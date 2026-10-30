#!/usr/bin/env bash
# qtclient install — build then stage into dist/cdn (dev tip-merge).
set -euo pipefail
OTC="$(cd "$(dirname "$0")/../.." && pwd)"
COMP="${COMP:-client}"
export COMP DEV="${DEV:-}" DEBUG="${DEBUG:-}"
bash "$OTC/scripts/lifecycle/build.sh"
case "$COMP" in
  client) python3 "$OTC/tools/publish/dev_cdn.py" --desktop ;;
  wasm) python3 "$OTC/tools/publish/dev_cdn.py" --web ;;
  launcher) : ;; # build is enough for stock launcher tip
  *) echo "[qtclient] install: unknown COMP=$COMP" >&2; exit 1 ;;
esac
