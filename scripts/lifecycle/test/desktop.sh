#!/usr/bin/env bash
# Stock desktop tests: gtest/ctest (when TIBIA_BUILD_TESTS) + session-dump smoke.
set -euo pipefail
QTCLIENT="$(cd "$(dirname "$0")/../../.." && pwd)"

# shellcheck source=../lib/desktop-paths.sh
source "$QTCLIENT/scripts/lifecycle/lib/desktop-paths.sh"
qtclient_desktop_paths "$QTCLIENT"

BUILD_DIR="${QTCLIENT_BUILD_DIR:-$QTCLIENT/build/$(qtclient_desktop_preset)}"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"

if [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  tests_on="$(grep -E '^TIBIA_BUILD_TESTS:BOOL=' "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2 || true)"
  if [[ "$tests_on" != "ON" ]]; then
    echo "[qtclient test] enabling TIBIA_BUILD_TESTS + vcpkg feature 'tests' on $BUILD_DIR"
    QT_PREFIX="$(bash "$QTCLIENT/tools/qt/qt-prefix.sh" 2>/dev/null || true)"
    cmake --preset "$QTCLIENT_PRESET" \
      -DTOGGLE_BIN_FOLDER=ON \
      ${QT_PREFIX:+-DCMAKE_PREFIX_PATH="$QT_PREFIX"} \
      -DTIBIA_BUILD_TESTS=ON \
      -DVCPKG_MANIFEST_FEATURES=tests
  fi

  # Build gtest executables only (skip OBJECT/phony compile-check helpers).
  # macOS ships Bash 3.2 (no mapfile); collect targets with a plain loop.
  test_targets=()
  while IFS= read -r t; do
    [[ -n "$t" ]] && test_targets+=("$t")
  done < <(
    ninja -C "$BUILD_DIR" -t targets all 2>/dev/null \
      | awk -F: '/CXX_EXECUTABLE_LINKER/ && /_tests:/ && $1 !~ /\.cmake$/ {
          n=$1; sub(".*/","",n); print n
        }' \
      || true
  )
  if ((${#test_targets[@]} > 0)); then
    echo "[qtclient test] building ${#test_targets[@]} gtest targets in $BUILD_DIR"
    cmake --build "$BUILD_DIR" -j"$JOBS" --target "${test_targets[@]}"
    echo "[qtclient test] ctest"
    ctest --test-dir "$BUILD_DIR" --output-on-failure -j"$JOBS"
  else
    echo "[qtclient test] no *_tests ninja targets — skip ctest" >&2
  fi
else
  echo "[qtclient test] no build tree at $BUILD_DIR — skip gtest (make stock client build DEV=1 first)" >&2
fi

# Lifecycle gate uses the synthetic EnterGame dump by default. Prefer play.cam
# explicitly (QTCLIENT_DUMP_FIXTURE=…/play.cam) when that replay is stable.
if [[ -z "${QTCLIENT_DUMP_FIXTURE:-}${QTCLIENT_SESSION_DUMP:-}" ]]; then
  export QTCLIENT_DUMP_FIXTURE="$QTCLIENT/testdata/sessiondumps/smoke.cam"
fi
bash "$QTCLIENT/scripts/ci/check-session-dump-smoke.sh"
