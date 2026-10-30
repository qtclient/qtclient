#!/usr/bin/env bash
# Configure tibia for iOS. Needs macOS and a Qt iOS kit (`make qt IOS=1`).
#
#   ./deploy/build-tibia-ios.sh
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OTC="${TIBIA_CLIENT_DIR:-$(cd "$HERE/.." && pwd)}"
CACHE_ENV="$(cd "$HERE/../../.." && pwd)/scripts/build-cache-env.sh"
if [[ -f "$CACHE_ENV" ]]; then
  # shellcheck disable=SC1090
  source "$CACHE_ENV"
fi
QT_PREFIX_SH="$OTC/tools/qt/qt-prefix.sh"

command -v cmake >/dev/null || { echo "[tibia-ios] need cmake"; exit 1; }
[[ "$(uname -s)" == Darwin ]] || { echo "[tibia-ios] macOS only"; exit 1; }

QT_IOS="$(bash "$QT_PREFIX_SH" --ios 2>/dev/null || true)"
QT_HOST="$(bash "$QT_PREFIX_SH")"
if [[ -z "$QT_IOS" || -z "$QT_HOST" ]]; then
  echo "[tibia-ios] Qt iOS kit missing — install with make qt IOS=1"
  exit 1
fi

BUILD_DIR="$OTC/build/tibia-ios"

echo "[tibia-ios] qt=$QT_IOS"
"$QT_IOS/bin/qt-cmake" -S "$OTC" -B "$BUILD_DIR" -G Xcode \
  -DQT_HOST_PATH="$QT_HOST" \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
  -DTIBIA_BUILD_APP=ON \
  -DTIBIA_BUILD_TESTS=OFF \
  -DTOGGLE_WEBGPU=OFF \
  -DOPTIONS_ENABLE_IPO=OFF

echo "[tibia-ios] configured → $BUILD_DIR (open in Xcode / cmake --build --target tibia)"
echo "[tibia-ios] TIBIA_MOBILE=1: TFullscreenLayout safe-area + density on orientation"
