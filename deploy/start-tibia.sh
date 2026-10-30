#!/usr/bin/env bash
set -euo pipefail
OTC="$(cd "$(dirname "$0")/.." && pwd)"
export COMP="${COMP:-client}"
if [[ "${1:-}" == "--check-qml" ]]; then
  shift
  exec bash "$OTC/scripts/lifecycle/check.sh" "$@"
fi
exec bash "$OTC/scripts/lifecycle/run.sh" "$@"
