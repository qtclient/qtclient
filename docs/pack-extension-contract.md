# Pack extension contract (CIP tip)

**Frozen** tip surface for product packs (out-of-tree `QTCLIENT_PACK` → `mods/<name>`).
Stock tip (`make stock`, `--no-mods`) must run without these hooks.

Public SDK: [`include/qtclient/`](../include/qtclient/README.md) — packs
`#include <qtclient/…>` only. No committed ALLOWLIST; `pack_sdk` ships that
tree plus tip closure (`tools/cip/sdk_tip_closure.py`).  
CI: [`check-cip-tip-purity.py`](../../../scripts/ci/check-cip-tip-purity.py),
[`check-cip-sdk-interface.py`](../../../scripts/ci/check-cip-sdk-interface.py),
the umbrella tip-include gate under `scripts/ci/`

Session accessors for packs: [`include/qtclient/session.h`](../include/qtclient/session.h)
→ `game/gamesession.h` (`localPlayer` / `isOnline` / `cipFeature` / `millis` /
`mapSeaFloor` / `worldLightIntensity` / `mapItemRevision` / world-time +
session-end subscribe / `requestBless`),
`game/thingsession.h` (item/race name + economy + `itemIsContainer` /
`itemIsPickupable` / `setItemThingShape` / effect-variant wrappers /
`raceOutfit` / `allRaceIds` / quest peels), `game/gamesession.h`
(`InventorySlotInfo` / `localPlayerInventorySlot`), `game/creaturesession.h`
(spectator / creature POD peels), and `game/mapsession.h`. Prefer these over
`world/game.h` / `assets/things.h` / `world/creature.h` / `world/localplayer.h`;
keep the Game hub include only for wire helpers. Raycast lighting uses the POD
`WorldLightingDriver` + `setItemThingShape` / gamesession peels — no tip-deep
`things.h` / `ThingType&` in product pack raycast TUs. Sprite peels:
`sprites/spritesession.h` (`QImage spriteImage`).

CMake: tip `INCLUDE_DIRECTORIES` + tip `COMPILE_DEFINITIONS`/`OPTIONS`
(notably `QT_NO_KEYWORDS`) always flow to linked packs via `QtClientMod.cmake`
(tip include dirs imply tip keyword/defs). `QTCLIENT_MOD_INTERNAL` is only the
`#ifdef` define when set — not the defs gate. `QTCLIENT_MOD_GUI_PRIVATE` =
GuiPrivate/RHI link only (see [`mods.md`](mods.md)). Do not require
`MOD_INTERNAL` just for RHI. Pack TUs use named / domain façades under
`include/qtclient/` named / domain façades only. Prefer
`QTCLIENT_PACK_SDK_ONLY=ON` (see [`docs/pack-architecture.md`](../../../docs/pack-architecture.md)).
Third-party / default packs (`otclient`) must `#include <qtclient/…>` only
(`check-pack-sdk-only.sh`). GuiPrivate remains for RHI packs.

## Public hooks

| Hook | Where | Role |
|------|-------|------|
| `qtclient/mod_api_v1.h` | `mods/mod_api_v1.*`, `mods/modloader.*` | Stable C ABI for pack load / suite wire |
| `qtclient/pack_host.h` | tip `main.cpp` fills `ModContext.packHost` | CIP-neutral wiring (quests, options, HUD panels) — prefer over stringly `services` |
| `qtclient_mod_<id>_register` / tip `…_entry` | pack TU + `GenerateLinkedMods.cmake` | Pack C++ registrar; tip-generated `extern "C"` entry; tip calls `qtclient_invoke_linked_mods` |
| `qtclient/clientoptions.h` | `app/clientoptions.h` | Pack JSON options store / notifier |
| `qtclient/dialog.h` | `gamewindow/dialogmanager.h` | Pack `TDialogController` / dialog stack |
| `qtclient/outfitpalette.h` | `qmlcomponents/outfitpalette.h` | Outfit colour palette leaf (no QML types) |
| `qtclient/extendedopcode_json.h` | `protocol/extendedopcode_json.h` | JSON extended-opcode send (no router hub) |
| `mods::setMissionBackend(MissionBackend*)` | `mods/registry.*` | Expedition / mission rows for quest tracker + quest log |
| `TQuestTrackerWidgetManager::setMissionBackend` | `gamewindow/questtrackerwidgetcontroller.*` | Tracker UI bind |
| `TQuestLogController::setMissionBackend` | `gamewindow/questlogcontroller.*` | Quest log bind |
| `TMinimapRendererQuickItem::setMissionZones` | `qmlcomponents/minimaprendererquickitem.*` | Mission zone overlays on minimap |
| `cipStatusTextInset` | Pack QML (`ServicesBar`, `AutomationBar`, …) | When `true`, speech-bubble overlay insets for pack HUD chrome |

## Rules

1. **No product HUD in** `client/tibia/` — packs only.
2. Hooks are **CIP-neutral** (ids, backends, QML properties) — never reintroduce
   OTC `ThingPtr` / `ItemPtr` keep-alives as the pack API.
3. Tip play path must not call into product pack symbols; packs plug hooks at load.
4. New pack needs → add a tip hook here and ratchet this doc; do not fork tip UX.
5. Do not add `TGameAction::object` / field UI `ThingPtr` / tip `UIWidget` — purity
   CI fails those regressions.

## Verify

```bash
python3 scripts/ci/check-cip-tip-purity.py
make stock client build          # tip without requiring pack
# with QTCLIENT_PACK / pack linked:
cmake --build apps/qtclient/build/macos-dev --target tibia
```

See also: [mods.md](mods.md) (pack load / FS layout).
