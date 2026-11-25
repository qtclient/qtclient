#!/usr/bin/env bash
# Breakpad symbols of a local desktop build into the local portal's symbol store,
# so crash reports sent to the local portal symbolicate. Every install replaces
# the binary, so its symbols must be kept now or that build's reports stay raw.
#   dump-symbols.sh <client binary> [<qt prefix>]
# Store: $QTCAAC_SYMBOLS_DIR, else <monorepo>/apps/portal/apps/web/data/telemetry/symbols
# (the portal's default QTCAAC_TELEMETRY_DIR/symbols), in the
# <module>/<debug_id>/<module>.sym layout `minidump-stackwalk --symbols-path` reads.
# Qt frameworks are dumped once per kit. Missing dump_syms only warns.
set -euo pipefail
BIN="${1:?usage: dump-symbols.sh <client binary> [<qt prefix>]}"
QT_PREFIX_DIR="${2:-}"
HERE="$(cd "$(dirname "$0")" && pwd)"
MONOREPO="$(cd "$HERE/../../../../.." && pwd)"
STORE="${QTCAAC_SYMBOLS_DIR:-$MONOREPO/apps/portal/apps/web/data/telemetry/symbols}"
if [[ -z "${QTCAAC_SYMBOLS_DIR:-}" && ! -d "$MONOREPO/apps/portal/apps/web" ]]; then
  exit 0 # no local portal next to this checkout
fi

DUMP_SYMS="$(command -v dump_syms || true)"
[[ -z "$DUMP_SYMS" && -x "$HOME/.cargo/bin/dump_syms" ]] && DUMP_SYMS="$HOME/.cargo/bin/dump_syms"
if [[ -z "$DUMP_SYMS" ]]; then
  echo "[symbols] dump_syms not found — local crash reports will not symbolicate (cargo install --locked dump_syms)" >&2
  exit 0
fi
[[ -f "$BIN" ]] || { echo "[symbols] no binary at $BIN" >&2; exit 0; }
mkdir -p "$STORE"

if "$DUMP_SYMS" -s "$STORE" "$BIN" >/dev/null 2>&1; then
  echo "[symbols] $(basename "$BIN") → $STORE"
else
  echo "[symbols] dump_syms failed for $BIN" >&2
fi

# Qt: the kit never changes under its prefix, so one dump per kit.
[[ -n "$QT_PREFIX_DIR" && -d "$QT_PREFIX_DIR/lib" ]] || exit 0
kit_id="$(printf '%s' "$QT_PREFIX_DIR" | shasum -a 256 | cut -c1-16)"
marker="$STORE/.qt-kit-$kit_id"
[[ -f "$marker" ]] && exit 0
n=0
for fw in "$QT_PREFIX_DIR"/lib/*.framework; do
  name="$(basename "$fw" .framework)"
  lib="$fw/Versions/A/$name"
  [[ -f "$lib" ]] || continue
  src="$lib"
  [[ -d "$QT_PREFIX_DIR/lib/$name.framework.dSYM" ]] && src="$QT_PREFIX_DIR/lib/$name.framework.dSYM"
  "$DUMP_SYMS" -s "$STORE" "$src" >/dev/null 2>&1 && n=$((n + 1))
done
for lib in "$QT_PREFIX_DIR"/plugins/*/*.dylib "$QT_PREFIX_DIR"/lib/*.dylib; do
  [[ -f "$lib" ]] || continue
  "$DUMP_SYMS" -s "$STORE" "$lib" >/dev/null 2>&1 && n=$((n + 1))
done
echo "$QT_PREFIX_DIR" > "$marker"
echo "[symbols] Qt kit: $n modules → $STORE"
