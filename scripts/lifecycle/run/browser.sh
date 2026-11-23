#!/usr/bin/env bash
# Component alias: make browser run → wasm lifecycle script.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
exec bash "$HERE/wasm.sh" "$@"
