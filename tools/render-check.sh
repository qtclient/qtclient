#!/usr/bin/env bash
# No-server tile render check: replays testdata/sessiondumps/fields.cam (grounds,
# a coin pile, a backpack under a flower, the player's outfit and an effect, all
# from the real appearances / spritesheets in data/assets), screenshots the client
# and compares the world map pane with fields.golden.png.
#
#   tools/render-check.sh            compare (exit 1 on a difference)
#   tools/render-check.sh --update   write a new golden
#
# QTCLIENT_TEST_PATHS=1 keeps settings and data in ~/.qttest (never the real
# Application Support / Preferences). Needs python3 with Pillow.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CLIENT="${QTCLIENT_BIN:-$ROOT/build/macos-dev/bin/client.app/Contents/MacOS/client}"
DUMPS="$ROOT/testdata/sessiondumps"
OUT="${RENDER_CHECK_OUT:-$(mktemp -d)}"
PREFIX="$(bash "$ROOT/tools/qt/qt-prefix.sh")"
python3 "$DUMPS/make_fields_cam.py" > "$OUT/fields.cam"
QTCLIENT_TEST_PATHS=1 QT_PLUGIN_PATH="$PREFIX/plugins" QML2_IMPORT_PATH="$PREFIX/qml" QML_IMPORT_PATH="$PREFIX/qml" \
    "$CLIENT" --no-mods --replay="$OUT/fields.cam" --screenshot-delay=1500 --screenshot="$OUT/fields.png" \
    > "$OUT/client.log" 2>&1
python3 - "$OUT/fields.png" "$DUMPS/fields.golden.png" "${1:-}" "$OUT" <<'PY'
import sys
from PIL import Image, ImageChops
shot, golden, mode, out = sys.argv[1:5]
# The world map pane of the 1020x650 window.
pane = Image.open(shot).convert('RGB').crop((94, 59, 575, 412))
if mode == '--update':
    pane.save(golden)
    print('render-check: golden updated')
    sys.exit(0)
diff = ImageChops.difference(pane, Image.open(golden).convert('RGB'))
# The creature name's brightness follows the light map's moving clouds: masked.
diff.paste((0, 0, 0), (199, 138, 283, 155))
box = diff.getbbox()
if box is None:
    print('render-check: OK (pixel-identical outside the name label)')
    sys.exit(0)
count = sum(1 for p in diff.getdata() if p != (0, 0, 0))
pane.save(out + '/pane.png')
diff.point(lambda v: 255 if v else 0).save(out + '/diff.png')
print(f'render-check: {count} pixels differ in {box}; see {out}/pane.png and {out}/diff.png')
sys.exit(1)
PY
