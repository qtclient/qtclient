#!/usr/bin/env bash
# qtclient build — COMP=client|launcher|wasm
set -euo pipefail
OTC="$(cd "$(dirname "$0")/../.." && pwd)"
COMP="${COMP:-client}"
flags=()
case "${DEV:-}" in 1|true|yes|on) flags+=(--dev) ;; esac
case "${DEBUG:-}" in 1|true|yes|on) flags+=(--debug) ;; esac

case "$COMP" in
  client)
    test -f "${VCPKG_ROOT:-$HOME/vcpkg}/scripts/buildsystems/vcpkg.cmake" || {
      echo "[qtclient] need VCPKG_ROOT" >&2
      exit 1
    }
    bash "$OTC/deploy/build-tibia.sh" "${flags[@]}"
    ;;
  launcher)
    bash "$OTC/deploy/build-tibia.sh" --launcher "${flags[@]}"
    ;;
  wasm)
    test -f "${VCPKG_ROOT:-$HOME/vcpkg}/scripts/buildsystems/vcpkg.cmake" || {
      echo "[qtclient] need VCPKG_ROOT" >&2
      exit 1
    }
    wasm_flags=(--wasm)
    case "${DEBUG:-}${DEV:-}" in *1*|*true*|*yes*|*on*) wasm_flags+=(--debug) ;; esac
    bash "$OTC/deploy/build-tibia.sh" "${wasm_flags[@]}"
    ;;
  *)
    echo "[qtclient] build: unknown COMP=$COMP" >&2
    exit 1
    ;;
esac
