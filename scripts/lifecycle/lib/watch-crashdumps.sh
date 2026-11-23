#!/usr/bin/env bash
# Dev-only: report client crashes in the terminal of `make … desktop run`.
# The launcher starts the client detached, so a segfault never reaches this
# shell, and CipSoft's crash handler only writes <id>.dmp (+ .json/.session) to
# <AppData>/crashdump — it logs nothing (host log parity). This polls those
# folders while the launcher or a client process is alive and prints, per new
# dump, its path and the crashing thread (symbolicated with the local portal's
# symbol store when minidump-stackwalk is installed).
#   watch-crashdumps.sh <launcher pid>
set -uo pipefail
LAUNCHER_PID="${1:?usage: watch-crashdumps.sh <launcher pid>}"
HERE="$(cd "$(dirname "$0")" && pwd)"
MONOREPO="$(cd "$HERE/../../../../.." && pwd)"
STORE="${QTCAAC_SYMBOLS_DIR:-$MONOREPO/apps/qtc-aac/apps/web/data/telemetry/symbols}"
STACKWALK="$(command -v minidump-stackwalk || true)"
[[ -z "$STACKWALK" && -x "$HOME/.cargo/bin/minidump-stackwalk" ]] && STACKWALK="$HOME/.cargo/bin/minidump-stackwalk"
FRAMES="${QTCLIENT_CRASH_FRAMES:-25}"

marker="$(mktemp -t qtclient-crashwatch.XXXXXX)"
trap 'rm -f "$marker"' EXIT
seen=" "

report() {
  local dmp="$1"
  echo "" >&2
  echo "[crash] client crashed — $dmp" >&2
  if [[ -f "$dmp.json" ]]; then
    python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print("[crash]", " ".join(f"{k}={d[k]}" for k in ("Version","LoginHost","Position") if k in d))' \
      "$dmp.json" >&2 2>/dev/null || true
  fi
  if [[ -z "$STACKWALK" ]]; then
    echo "[crash] install minidump-stackwalk for a stack here (cargo install --locked minidump-stackwalk)" >&2
    return
  fi
  local args=(--brief)
  [[ -d "$STORE" ]] && args+=(--symbols-path "$STORE")
  "$STACKWALK" "${args[@]}" "$dmp" 2>/dev/null |
    awk -v max="$FRAMES" '
      /^Crash (reason|address):/ { print "[crash] " $0; next }
      /^Thread .*\(crashed\)/ { t = 1; print "[crash] " $0; next }
      t && /^ *[0-9]+  / { if (n++ < max) print "[crash] " $0 }
    ' >&2
}

alive() {
  kill -0 "$LAUNCHER_PID" 2>/dev/null && return 0
  pgrep -u "$(id -u)" -x client >/dev/null 2>&1
}

while :; do
  sleep 2
  for dir in "$HOME/Library/Application Support"/*/*/crashdump "${XDG_DATA_HOME:-$HOME/.local/share}"/*/*/crashdump; do
    [[ -d "$dir" ]] || continue
    while IFS= read -r dmp; do
      [[ "$seen" == *" $dmp "* ]] && continue
      seen+="$dmp "
      sleep 1 # the .json / .session are written right after the .dmp
      report "$dmp"
    done < <(find "$dir" -maxdepth 1 -name '*.dmp' -newer "$marker" 2>/dev/null)
  done
  alive || break
done
