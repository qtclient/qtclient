#!/usr/bin/env bash
# Replay a checked-in packet dump with tibia --screenshot; fail on crash,
# missing world entry, or a tiny/missing PNG.
#
# Fixture preference (first existing file wins):
#   1. QTCLIENT_DUMP_FIXTURE or QTCLIENT_SESSION_DUMP (explicit override)
#   2. testdata/sessiondumps/play.cam (richer modern-protocol dump when present)
#   3. testdata/sessiondumps/smoke.cam (tiny synthetic EnterGame oracle)
#
# Env:
#   QTCLIENT_CLIENT_BIN     path to tibia binary (optional; auto-detect)
#   QTCLIENT_DUMP_FIXTURE  dump path override (alias: QTCLIENT_SESSION_DUMP)
#   QTCLIENT_SCREENSHOT    output PNG (default: under TMPDIR)
#   QTCLIENT_SMOKE_TIMEOUT seconds (default: 30)
#   QTCLIENT_MIN_PNG_BYTES minimum PNG size (default: 1024)
#   QTCLIENT_SKIP_IF_NO_ASSETS  if 1, exit 0 when data/assets is missing (CI)
#   QTCLIENT_SCREENSHOT_BASELINE  optional PNG; when set, require matching IHDR
#   QTCLIENT_SCREENSHOT_ORACLE    oracle file (default: testdata/.../play-baseline.oracle
#     when dump is play.cam). Compares IHDR + 8x8 average-hash (max_hamming).
#   QTCLIENT_SKIP_ORACLE          if 1, skip oracle compare even when present
set -euo pipefail

QTCLIENT="$(cd "$(dirname "$0")/../.." && pwd)"
CALLER_PWD="$PWD"
cd "$QTCLIENT"

# Paths from the environment are relative to where the script was called from.
absolute() {
  case "$1" in
    /*) echo "$1" ;;
    *) echo "$CALLER_PWD/$1" ;;
  esac
}

resolve_dump() {
  if [[ -n "${QTCLIENT_DUMP_FIXTURE:-}" ]]; then
    absolute "$QTCLIENT_DUMP_FIXTURE"
    return
  fi
  if [[ -n "${QTCLIENT_SESSION_DUMP:-}" ]]; then
    absolute "$QTCLIENT_SESSION_DUMP"
    return
  fi
  local candidate
  for candidate in \
    "$QTCLIENT/testdata/sessiondumps/play.cam" \
    "$QTCLIENT/testdata/sessiondumps/smoke.cam"; do
    # -e follows symlinks; require a resolvable dump file.
    if [[ -e "$candidate" && -f "$candidate" ]]; then
      echo "$candidate"
      return
    fi
  done
  echo "$QTCLIENT/testdata/sessiondumps/smoke.cam"
}

DUMP="$(resolve_dump)"
OUT="${QTCLIENT_SCREENSHOT:-${TMPDIR:-/tmp}/tibia-session-dump-smoke.png}"
TIMEOUT_S="${QTCLIENT_SMOKE_TIMEOUT:-30}"
MIN_BYTES="${QTCLIENT_MIN_PNG_BYTES:-1024}"
SKIP_NO_ASSETS="${QTCLIENT_SKIP_IF_NO_ASSETS:-0}"

if [[ ! -f "$DUMP" ]]; then
  echo "[session-dump-smoke] missing dump: $DUMP" >&2
  exit 1
fi

if [[ ! -f "$QTCLIENT/data/assets/catalog-content.json" ]]; then
  if [[ "$SKIP_NO_ASSETS" == "1" ]]; then
    echo "[session-dump-smoke] skip: no CIP assets (make extract TIBIA=…)"
    exit 0
  fi
  echo "[session-dump-smoke] need CIP assets under data/assets/ (make extract TIBIA=…)" >&2
  exit 1
fi

# Resolve the tip binary and Qt kit paths in this shell. find-client-bin alone
# runs in a subshell, so QT_PLUGIN_PATH (needed for offscreen) would be lost.
# shellcheck source=../lifecycle/lib/desktop-paths.sh
source "$QTCLIENT/scripts/lifecycle/lib/desktop-paths.sh"
qtclient_desktop_paths "$QTCLIENT" || true

BIN="${QTCLIENT_CLIENT_BIN:-}"
if [[ -z "$BIN" || ! -e "$BIN" ]]; then
  FIND_BIN="$QTCLIENT/scripts/lifecycle/lib/find-client-bin.sh"
  BIN="$("$FIND_BIN" "$QTCLIENT" 2>/dev/null || true)"
fi
if [[ -z "${BIN:-}" || ! -x "$BIN" ]]; then
  echo "[session-dump-smoke] client binary not found — set QTCLIENT_CLIENT_BIN or run make client-build" >&2
  exit 1
fi

# Non-macdeployqt tips (macos-dev / linux-dev) need the kit's platforms/;
# release .app may also miss offscreen unless plugins are next to the binary.
if [[ -z "${QT_PLUGIN_PATH:-}" ]]; then
  qt_prefix=""
  if [[ -x "$QTCLIENT/tools/qt/qt-prefix.sh" ]]; then
    qt_prefix="$(bash "$QTCLIENT/tools/qt/qt-prefix.sh" 2>/dev/null || true)"
  fi
  if [[ -n "$qt_prefix" && -d "$qt_prefix/plugins" ]]; then
    export QT_PLUGIN_PATH="$qt_prefix/plugins"
  fi
fi

DUMP_ABS="$(cd "$(dirname "$DUMP")" && pwd)/$(basename "$DUMP")"
OUT_ABS="$(mkdir -p "$(dirname "$OUT")" && cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")"
rm -f "$OUT_ABS"

echo "[session-dump-smoke] bin=$BIN"
echo "[session-dump-smoke] dump=$DUMP_ABS"
echo "[session-dump-smoke] out=$OUT_ABS"

run_tibia() {
  # Prefer coreutils timeout when present (Linux CI); otherwise perl alarm (macOS).
  local -a cmd=(
    env
    QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
    QT_LOGGING_RULES="${QT_LOGGING_RULES:-tibia.gameclient=true;tibia.sessiondump=true}"
    "$BIN"
    --renderer=software
    --replay="$DUMP_ABS"
    --screenshot="$OUT_ABS"
    --screenshot-delay=500
  )
  if command -v timeout >/dev/null 2>&1; then
    timeout --signal=KILL "${TIMEOUT_S}s" "${cmd[@]}"
  else
    perl -e 'alarm shift; exec @ARGV' "$TIMEOUT_S" "${cmd[@]}"
  fi
}

set +e
run_tibia
rc=$?
set -e

if [[ "$rc" -eq 124 || "$rc" -eq 142 ]]; then
  # GNU timeout → 124; perl SIGALRM → 142
  echo "[session-dump-smoke] timed out after ${TIMEOUT_S}s (missing world entry or hung replay)" >&2
  exit 1
fi
if [[ "$rc" -ne 0 ]]; then
  echo "[session-dump-smoke] tibia exited $rc (crash or fail(); replay errors exit 2)" >&2
  exit "$rc"
fi

if [[ ! -f "$OUT_ABS" ]]; then
  echo "[session-dump-smoke] screenshot missing: $OUT_ABS (world never entered?)" >&2
  exit 1
fi

size="$(wc -c <"$OUT_ABS" | tr -d ' ')"
if [[ "$size" -lt "$MIN_BYTES" ]]; then
  echo "[session-dump-smoke] PNG too small: ${size} bytes (min ${MIN_BYTES})" >&2
  exit 1
fi

magic="$(head -c 8 "$OUT_ABS" | od -An -tx1 | tr -d ' \n')"
if [[ "$magic" != "89504e470d0a1a0a" ]]; then
  echo "[session-dump-smoke] not a PNG (bad magic): $OUT_ABS" >&2
  exit 1
fi

png_ihdr() {
  # Print "WIDTHxHEIGHT" from PNG IHDR (bytes 16..23 big-endian).
  python3 - "$1" <<'PY'
import struct, sys
with open(sys.argv[1], "rb") as f:
    sig = f.read(8)
    if sig != b"\x89PNG\r\n\x1a\n":
        raise SystemExit("not png")
    length, typ = struct.unpack(">I4s", f.read(8))
    if typ != b"IHDR" or length < 8:
        raise SystemExit("no ihdr")
    w, h = struct.unpack(">II", f.read(8))
    print(f"{w}x{h}")
PY
}

if [[ -n "${QTCLIENT_SCREENSHOT_BASELINE:-}" ]]; then
  if [[ ! -f "$QTCLIENT_SCREENSHOT_BASELINE" ]]; then
    echo "[session-dump-smoke] baseline missing: $QTCLIENT_SCREENSHOT_BASELINE" >&2
    exit 1
  fi
  got="$(png_ihdr "$OUT_ABS")"
  want="$(png_ihdr "$QTCLIENT_SCREENSHOT_BASELINE")"
  if [[ "$got" != "$want" ]]; then
    echo "[session-dump-smoke] pixel-oracle size mismatch: got $got want $want" >&2
    exit 1
  fi
  echo "[session-dump-smoke] pixel-oracle IHDR ok ($got)"
fi

ORACLE="${QTCLIENT_SCREENSHOT_ORACLE:-}"
if [[ -z "$ORACLE" && "$(basename "$DUMP_ABS")" == "play.cam" ]]; then
  ORACLE="$QTCLIENT/testdata/sessiondumps/play-baseline.oracle"
fi
if [[ "${QTCLIENT_SKIP_ORACLE:-0}" != "1" && -n "$ORACLE" && -f "$ORACLE" ]]; then
  echo "[session-dump-smoke] oracle=$ORACLE"
  python3 "$QTCLIENT/scripts/ci/compare-png-oracle.py" "$OUT_ABS" "$ORACLE"
fi

echo "[session-dump-smoke] ok (${size} bytes) $OUT_ABS"
