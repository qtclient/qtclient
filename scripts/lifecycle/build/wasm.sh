#!/usr/bin/env bash
# Build stock MAIN_MODULE wasm client.
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
exec bash "$HERE/desktop.sh" --wasm "$@"
