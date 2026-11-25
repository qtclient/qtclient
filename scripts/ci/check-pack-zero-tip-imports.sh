#!/usr/bin/env bash
# Gate: pack C++ must not import tip symbols.
#   desktop MODULE:  --mod <native/mod.dylib|so|dll>
#   wasm STATIC_ABI: --archive <libqtclient_mod_<id>.a> [--id <id>]
#     (the final tibia.wasm link would satisfy a tip import, so the archive is
#      checked on its own; its own definitions are excluded)
# Usage: check-pack-zero-tip-imports.sh (--mod FILE | --archive FILE [--id ID]) [--tip-nm tip.syms]
set -euo pipefail

MOD=""
ARCHIVE=""
ID=""
TIP_NM=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --mod) MOD=$2; shift 2 ;;
    --archive) ARCHIVE=$2; shift 2 ;;
    --id) ID=$2; shift 2 ;;
    --tip-nm) TIP_NM=$2; shift 2 ;;
    *) echo "usage: $0 (--mod native/mod.* | --archive lib.a [--id ID]) [--tip-nm tip.syms]" >&2; exit 2 ;;
  esac
done
if [[ -n "$ARCHIVE" ]]; then
  [[ -f "$ARCHIVE" ]] || { echo "missing --archive $ARCHIVE" >&2; exit 1; }
  MOD="$ARCHIVE"
fi
[[ -n "$MOD" && -f "$MOD" ]] || { echo "missing --mod/--archive" >&2; exit 1; }

# Resolve undefined symbols (platform nm; emnm for wasm objects when present).
NM=""
for cand in llvm-nm emnm nm; do
  if command -v "$cand" >/dev/null 2>&1; then NM="$cand"; break; fi
done
if [[ -z "$NM" ]]; then
  echo "check-pack-zero-tip-imports: nm not found (skip)" >&2
  exit 0
fi
if [[ -n "$ARCHIVE" ]]; then
  # Archive: undefined minus what any member defines (intra-pack references).
  defined=$("$NM" --defined-only "$ARCHIVE" 2>/dev/null | awk 'NF>=3 {print $NF}' | sort -u || true)
  undef=$("$NM" -u "$ARCHIVE" 2>/dev/null | awk 'NF && $0 !~ /:$/ {print $NF}' | sort -u \
    | comm -23 - <(printf '%s\n' "$defined") || true)
  # The archive must define its static-ABI entry.
  if [[ -n "$ID" ]]; then
    c_id="$(echo "$ID" | tr -c 'A-Za-z0-9_\n' '_')"
    if ! printf '%s\n' "$defined" | grep -Eq "^_?qtclient_mod_${c_id}_register_abi$"; then
      echo "check-pack-zero-tip-imports: $ARCHIVE does not define qtclient_mod_${c_id}_register_abi" >&2
      exit 1
    fi
  fi
else
  undef=$("$NM" -u "$MOD" 2>/dev/null | awk '{print $NF}' | grep -v '^$' || true)
fi

# Always allow CRT / C++ ABI / compiler builtins.
allow_re='^(_?_?(Zn|Zna|Zdl|Zda|ZT|ZS|ZTI|ZTS|ZTV|NV|TI|TS|cxa|gxx|GCC|emscripten|wasi|__stack|__wasm|memcpy|memmove|memset|memcmp|strlen|malloc|free|calloc|realloc|abort|stderr|fprintf|snprintf|strcmp|strncpy|pthread|dyld|objc))'

bad=0
while IFS= read -r sym; do
  [[ -z "$sym" ]] && continue
  if echo "$sym" | grep -Eq "$allow_re"; then
    continue
  fi
  # Tip symbols: tibia::* / cip::* mangled (N5tibia, _ZN5tibia, N3cip), the old
  # qtclient_host_api_v1() fallback, absorb entries, tip globals.
  if echo "$sym" | grep -Eq 'tibia|N3cip|qtclient_host_api|qtclient_mod_host|qtclient_invoke_linked|g_game|g_map'; then
    echo "check-pack-zero-tip-imports: tip import '$sym' in $MOD" >&2
    bad=1
    continue
  fi
  if [[ -n "$TIP_NM" && -f "$TIP_NM" ]]; then
    if grep -Fxq "$sym" "$TIP_NM"; then
      echo "check-pack-zero-tip-imports: undefined $sym resolves in tip ($MOD)" >&2
      bad=1
    fi
  fi
done <<< "$undef"

if [[ "$bad" -ne 0 ]]; then
  exit 1
fi
echo "check-pack-zero-tip-imports: ok ($MOD)"
