#!/usr/bin/env bash
set -euo pipefail
OTC="$(cd "$(dirname "$0")/.." && pwd)"
export PACKAGE_KIND=wasm WASM_BUILD="${1:?}" PACKAGE_OUT="${2:?}"
exec bash "$OTC/scripts/lifecycle/package.sh"
