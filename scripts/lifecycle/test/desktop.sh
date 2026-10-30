#!/usr/bin/env bash
set -euo pipefail
OTC="$(cd "$(dirname "$0")/../../.." && pwd)"
bash "$OTC/scripts/ci/check-session-dump-smoke.sh"
