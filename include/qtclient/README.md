# qtclient pack SDK interface

**Public API for product / third-party packs.** Packs must `#include <qtclient/…>` only.

There is **no** `sdk/ALLOWLIST`. Extending the surface means adding a header here;
`pack_sdk` ships this tree plus a tip closure computed at pack time
(`tools/cip/sdk_tip_closure.py`).

| Header | Role |
|--------|------|
| `mod_api_v1.h` / `mod.h` | Host Mod API (named façades only) |
| `mod_entry.h` | Tip linked-pack entry / invoke decls (pack TUs use `…_register`) |
| `mod_registry.h` | Identity / backends / effect hooks / pack QML paths |
| `pack_host.h` | Tip-filled wiring surface |
| `worldview.h` | Lighting driver, overlays, frame hooks, lighting PODs |
| `weather.h` | Weather driver hook |
| `hdscale.h` | HD sprite scaler registration |
| `session.h` | Game / creature / map / things session peels |
| `cooldown.h` | Spell / attack cooldown storage |
| `const.h` | CIP client constants / enums |
| `gameactions.h` | Game-action publish / handler hooks |
| `notify.h` | Game notify hooks |
| `host.h` | Stock host identity flags |
| `clientoptions.h` | Client options JSON store |
| `dialog.h` | Dialog stack / `TDialogController` |
| `outfitpalette.h` | Outfit colour palette leaf |
| `extendedopcode.h` / `extendedopcode_json.h` | Extended opcode reader + JSON send |
| `gamewindow/…`, `minimap/…`, … | Domain façades (CIP tip layout) |

Do **not** `#include "gamewindow/…"`, `"input/…"`, or `"tibia/…"` from pack code —
use the named or domain façade under `<qtclient/…>`.
