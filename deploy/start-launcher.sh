#!/usr/bin/env bash
# Start the stock CIP tip launcher (cwd = apps/qtclient).
# Binary: build/<preset>/bin/launcher[.app|/…] — see desktop-paths.sh
# stock qtclient: --no-mods (same idea as client). IdleOT brand: tip mods/<id>/
# after `make idleot launcher install` (no compile-time brand flip).
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="${TIBIA_CLIENT_DIR:-$(cd "$HERE/.." && pwd)}"
ROOT="$(cd "$OTC/../.." && pwd)"
DEV_JSON="$ROOT/dist/cdn/latest/dev.json"

# shellcheck source=desktop-paths.sh
source "$HERE/desktop-paths.sh"
qtclient_desktop_paths "$OTC"

if [[ ! -x "$QTCLIENT_LAUNCHER_BIN" ]]; then
  echo "[launcher] missing $QTCLIENT_LAUNCHER_BIN — make qtclient launcher build" >&2
  exit 1
fi

# Login URLs: tip pack / QTCLIENT_MODS_DIR / --default-mod; --no-mods → CipSoft.
if [[ -f "$DEV_JSON" ]]; then
  export QTCLIENT_DEV_JSON="$DEV_JSON"
elif [[ -x "$QTCLIENT_CLIENT_BIN" ]]; then
  # Point Play at the qtclient build even before `make cdn` / dev_cdn.py.
  mkdir -p "$(dirname "$DEV_JSON")"
  # Prefer live release.json revision so loginservice.php does not return errorCode 7.
  REV=$(python3 -c 'import json,pathlib; p=pathlib.Path(r"'"$ROOT"'/dist/cdn/latest/release.json");
print((json.loads(p.read_text()).get("revision") or "dev") if p.is_file() else "dev")' 2>/dev/null || echo dev)
  python3 -c 'import json,os,sys; json.dump({"desktop":{"exe":sys.argv[1],"cwd":sys.argv[2],"revision":sys.argv[4]}}, open(sys.argv[3],"w")); print("[launcher] wrote", sys.argv[3], "revision="+sys.argv[4])' \
    "$QTCLIENT_CLIENT_BIN" "$OTC" "$DEV_JSON" "$REV"
  export QTCLIENT_DEV_JSON="$DEV_JSON"
fi
if [[ ! -f "${QTCLIENT_DEV_JSON:-}" && ! -x "$QTCLIENT_CLIENT_BIN" ]]; then
  echo "[launcher] no qtclient client — make qtclient client build (Play needs dist/cdn/latest/dev.json)" >&2
fi
# qtclient Play → client --no-mods (also passed as launcher argv).
export QTCLIENT_CLIENT_ARGS="${QTCLIENT_CLIENT_ARGS:---no-mods}"

echo "[launcher] starting $QTCLIENT_LAUNCHER_BIN (stock qtclient; Play → qtclient client)"
cd "$OTC"
exec "$QTCLIENT_LAUNCHER_BIN" --no-mods "$@"
