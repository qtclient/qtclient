#!/usr/bin/env bash
set -euo pipefail
OTC="$(cd "$(dirname "$0")/.." && pwd)"
export COMP=launcher
exec bash "$OTC/scripts/lifecycle/run.sh" "$@"
