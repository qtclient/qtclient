#!/usr/bin/env bash
# Batch DecompileTree for CIP clone Phase 0. Absolute paths; read-only Ghidra project.
# Usage: tools/re/decompile_cip_subsystems.sh [qsg|input|render|all]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GH="${GHIDRA_INSTALL_DIR:-$(brew --prefix ghidra)/libexec}/support/analyzeHeadless"
PRIV="$ROOT/tmp/re/tibia/ghidra-clone"
NAMES="$ROOT/tmp/re/tibia/index/names.txt"
SCRIPTS="$ROOT/tools/re/ghidra"
need_project() {
  if [[ ! -d "$PRIV/tibia.rep" ]]; then
    echo "copy ghidra project to $PRIV first" >&2
    exit 1
  fi
}
run_tree() {
  local out="$1" depth="$2" max="$3"
  shift 3
  mkdir -p "$out"
  echo "[decompile] -> $out ($*)"
  "$GH" "$PRIV" tibia -process client -noanalysis -readOnly \
    -scriptPath "$SCRIPTS" \
    -postScript DecompileTree.java "$out" "$depth" "$max" "$NAMES" "$@" \
    >"${out%/tree}/decomp.log" 2>&1 || true
  echo "[decompile] files=$(ls "$out" 2>/dev/null | wc -l | tr -d ' ')"
}
need_project
AREA="${1:-all}"
case "$AREA" in
  qsg|all)
    run_tree "$ROOT/tmp/re/tibia/render/qsg/tree" 3 250 \
      0x1010c2d10 0x1010ce2b0 0x1010b5d90 0x1010b5f70 0x1010567b0 0x101056cd0
    ;;&
  input|all)
    mapfile -t roots < <(rg -o '^0x\S+(?= \S*::TWorldMapPicker::)' -P "$NAMES" | sort -u | head -12)
    run_tree "$ROOT/tmp/re/tibia/input/tree" 2 120 "${roots[@]}"
    ;;&
  render|all)
    mapfile -t roots < <(rg -o '^0x\S+(?= \S*::TWorldMapRenderProvider::)' -P "$NAMES" | sort -u | head -24)
    run_tree "$ROOT/tmp/re/tibia/render/tree" 3 400 "${roots[@]}"
    ;;&
  *) ;;
esac
echo "[decompile] done"
