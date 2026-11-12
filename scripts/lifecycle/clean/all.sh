#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
bash "$HERE/desktop.sh"
bash "$HERE/launcher.sh"
bash "$HERE/wasm.sh"
