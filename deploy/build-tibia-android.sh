#!/usr/bin/env bash
# Build tibia for Android (Phase 6).
# Needs: make qt ANDROID=1, ANDROID_SDK_ROOT, ANDROID_NDK_ROOT.
#
#   ./deploy/build-tibia-android.sh
#   ./deploy/build-tibia-android.sh --abi arm64-v8a
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="${TIBIA_CLIENT_DIR:-$(cd "$HERE/.." && pwd)}"
CACHE_ENV="$(cd "$HERE/../../.." && pwd)/scripts/build-cache-env.sh"
if [[ -f "$CACHE_ENV" ]]; then
  # shellcheck disable=SC1090
  source "$CACHE_ENV"
fi
QT_PREFIX_SH="$OTC/tools/qt/qt-prefix.sh"
ABI="${ANDROID_ABI:-arm64-v8a}"
DEBUG_BUILD=0

for arg in "$@"; do
  case "$arg" in
    --abi=*) ABI="${arg#--abi=}" ;;
    --abi) shift; ABI="${1:-$ABI}" ;;
    --debug|--dev) DEBUG_BUILD=1 ;;
    -h|--help)
      sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
  esac
done

command -v cmake >/dev/null || { echo "[tibia-android] need cmake"; exit 1; }
command -v ninja >/dev/null || { echo "[tibia-android] need ninja"; exit 1; }
: "${ANDROID_SDK_ROOT:=${ANDROID_HOME:-}}"
: "${ANDROID_NDK_ROOT:=${ANDROID_NDK_HOME:-}}"
[[ -n "$ANDROID_SDK_ROOT" ]] || { echo "[tibia-android] set ANDROID_SDK_ROOT"; exit 1; }
[[ -n "$ANDROID_NDK_ROOT" ]] || { echo "[tibia-android] set ANDROID_NDK_ROOT"; exit 1; }

QT_ANDROID="$(bash "$QT_PREFIX_SH" --android 2>/dev/null || true)"
QT_HOST="$(bash "$QT_PREFIX_SH")"
if [[ -z "$QT_ANDROID" || -z "$QT_HOST" ]]; then
  echo "[tibia-android] Qt Android kit missing — install with: make qt ANDROID=1"
  exit 1
fi

BUILD_TYPE=Release
[[ "$DEBUG_BUILD" -eq 1 ]] && BUILD_TYPE=Debug
BUILD_DIR="$OTC/build/tibia-android-$ABI"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc)"

echo "[tibia-android] abi=$ABI qt=$QT_ANDROID sdk=$ANDROID_SDK_ROOT"
"$QT_ANDROID/bin/qt-cmake" -S "$OTC" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DQT_HOST_PATH="$QT_HOST" \
  -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
  -DANDROID_NDK="$ANDROID_NDK_ROOT" \
  -DANDROID_ABI="$ABI" \
  -DANDROID_PLATFORM=android-28 \
  -DTIBIA_BUILD_APP=ON \
  -DTIBIA_BUILD_TESTS=OFF \
  -DTOGGLE_WEBGPU=OFF \
  -DOPTIONS_ENABLE_IPO=OFF

cmake --build "$BUILD_DIR" --target tibia -j"$JOBS"
echo "[tibia-android] ok → $BUILD_DIR (apk via androiddeployqt / Qt Creator)"
echo "[tibia-android] TIBIA_MOBILE=1: touch → map hover; TFullscreenLayout safe-area + density"
