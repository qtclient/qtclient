#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="${TIBIA_CLIENT_DIR:-$(cd "$HERE/../../.." && pwd)}"
# shellcheck source=../lib/desktop-paths.sh
source "$OTC/scripts/lifecycle/lib/desktop-paths.sh"
qtclient_desktop_paths "$OTC"

if [[ ! -x "$QTCLIENT_CLIENT_BIN" ]]; then
  echo "[tibia] missing $QTCLIENT_CLIENT_BIN — make qtclient client build (preset=$QTCLIENT_PRESET)" >&2
  exit 1
fi
if [[ -x "$QTCLIENT_LAUNCHER_BIN" ]]; then
  export QTCLIENT_LAUNCHER_BIN
  echo "[tibia] Manage Clients → $QTCLIENT_LAUNCHER_BIN"
fi
echo "[tibia] starting $QTCLIENT_CLIENT_BIN (cwd $OTC)"
cd "$OTC"
bash "$OTC/scripts/lifecycle/lib/link-cip-resources.sh" "$QTCLIENT_CLIENT_BIN" "$OTC/data" || true
if [[ -n "${ARGS+x}" ]]; then
  client_args=(${ARGS})
elif [[ -n "${QTCLIENT_MODS_DIR:-}" ]]; then
  client_args=()
else
  client_args=(--no-mods)
fi
# shellcheck disable=SC2086
exec "$QTCLIENT_CLIENT_BIN" "${client_args[@]}" "$@"
