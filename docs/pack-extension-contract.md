# Pack extension contract (CIP tip)

**Frozen** tip surface for product packs (out-of-tree `QTCLIENT_PACK` → `mods/<name>`).
Stock tip (`make stock`, `--no-mods`) must run without these hooks.

Pack SDK (host API v9): [`include/qtclient/abi/`](../include/qtclient/README.md)
only, shipped as the `QtClientSdk` package ([`sdk/`](../sdk/CMakeLists.txt): abi
headers + `QtClientMod.cmake` + a config recording ABI version, hostBuildId and Qt
kit). Packs `#include <qtclient/abi/…>` only — the non-abi `include/qtclient/*.h`
façades, the tip closure and `ABSORB_INTO_QTCLIENT` are gone. A pack builds
against the installed SDK alone and the host takes the installed pack
(`-DQTCLIENT_PACKS`); see [`mods.md`](mods.md) (Build).
CI: [`check-cip-tip-purity.py`](../../../scripts/ci/check-cip-tip-purity.py),
[`check-cip-sdk-interface.py`](../../../scripts/ci/check-cip-sdk-interface.py),
[`check-pack-abi-headers.py`](../../../scripts/ci/check-pack-abi-headers.py),
[`check-pack-sdk-build.sh`](../scripts/ci/check-pack-sdk-build.sh), and the
umbrella pack ratchets (`check-idleot-abi-progress.py`, `check-idleot-tip-includes.py`).

Host objects reach a pack only as typed tables, `QtClientHandle` tokens,
incomplete structs (`QtClientDialog`, `QtClientContextMenu`, `QtClientNotifyArgs`)
or ids; Qt objects as `QtClientQ*Ptr` typedefs. Raw `void*` is only a pack's
`user` / `self` (and `QtClientHandle.ptr`). Each table starts with `struct_size`;
later additions are tail-only (`QTCLIENT_ABI_HAS`). Host implementations live in
`client/tibia/mods/*_abi.cpp` over the CipSoft classes; no IdleOT names there.

Build: `qtclient_add_mod` gives the pack only its own dirs + the abi include dir
(no tip PCH or tip definitions; it defines `QT_NO_KEYWORDS`). `GUI_PRIVATE` =
Qt GuiPrivate/QuickPrivate for RHI scene-graph nodes (version-locked to the
host's Qt: `hostBuildId` + `api->qt_version`).

## v9 tables (`QtClientHostApiV1`)

| Table | Header | Host | Role |
|-------|--------|------|------|
| root | `abi/host_api_v1.h` | `mods/host_api_v1.cpp`, `mods/modloader.*` | `api_version` 9, `host_build_id`, `qt_version`; v1–v8 entries (register, hooks, world view, lighting); ModLoader passes it to `qtclient_mod_register` / `qtclient_mod_<c_id>_register_abi` |
| `QtClientModLoadCtx.game` | `abi/context.h` | `mods/context_abi.cpp` | `QtClientGameContextV1` (replaces the tip `ModContext` / `early_ctx`) |
| `game_host` | `abi/gamehost_api.h` | `mods/gamehost_abi.cpp` | game window / session lifecycle events |
| `opcode` | `abi/opcode_api.h` | `mods/opcode_abi.cpp` | extended opcodes (JSON send / receive) |
| `dialog` | `abi/dialog_api.h` | `mods/dialog_abi.cpp` | pack dialogs on the CipSoft dialog stack, message boxes, host dialogs, `set_host_dialog_qml` (create character) |
| `action` | `abi/action_api.h` | `mods/action_abi.cpp` | game actions by name + JSON (publish / subscribe), context-menu extensions, target selection |
| `chat` | `abi/chat_api.h` | `mods/chat_abi.cpp` | chat entries, NPC external conversation |
| `cooldown`, `game_data` | `abi/gamedata_api.h` | `mods/gamedata_abi.cpp` | cooldowns, spells, outfit palette, static game data |
| `ui` | `abi/ui_api.h` | `mods/ui_abi.cpp` | sidebars / widgets / minimap / map renderer items |
| `backends` | `abi/backends.h` | `mods/backend_adapters.cpp` | mission / attention / instance backends as C vtables; the host binds quest tracker, quest log, info dialogs and button bar |
| `cip_object` | `abi/cip_object_api.h` | `mods/cip_object_*`, `mods/cip_host_classes.h` | CipSoft classes by name: delegate `invoke` (moc name, JSON args), `call_base`, `set_value`, signals; `set_host_controller` |
| `pack_host` | `abi/pack_host_api.h` | `mods/pack_host_abi.cpp` | Qt-typed chrome items (minimap / inventory / button bar panels), active chat item, map overlay, options page (`QtClientOptionsPageV1`), store, logout |
| `notify` | `abi/notify_api.h` | `mods/notify_abi.cpp` | CipSoft field bus; `arg_count` / `arg_kind` / `arg_json` (payloads: [`mods.md`](mods.md#notify-payloads-notify-arg_json)); containers; quick-loot user edits (v9 tail) |
| `session` | `abi/session_api.h` | `mods/session_abi.cpp` | current session peels (containers, quick loot, imbuing, NPC trade, attack mark, map data) |
| `options`, `sound`, lighting | `abi/options_api.h`, `abi/sound_api.h`, `abi/lighting.h` | `mods/options_abi.cpp`, `sound/ambiencehold.*`, `mods/worldview.*` | options store, ambience channel, world lighting driver |

## Public hooks

| Hook | Where | Role |
|------|-------|------|
| `qtclient_mod_register` / `qtclient_mod_<c_id>_register_abi` | pack entry (`QTCLIENT_MOD_DEFINE_REGISTER`) | Sole pack symbol; desktop tip loads `native/mod.*`, wasm tip calls the static-ABI table (`static_abi_mods.cpp`) |
| `options` (`QtClientOptionsApiV1`) | `app/clientoptions.h` | Pack JSON options store / notifier |
| `dialog` | `gamewindow/dialogmanager.h` | Pack dialogs on the CipSoft dialog stack |
| `game_data` outfit palette | `qmlcomponents/outfitpalette.h` | Outfit colour palette (no QML types) |
| `opcode` | `protocol/extendedopcode_json.h` | JSON extended opcodes (no router hub) |
| `backends->set_mission_backend` (vtable) → `MissionBackend` | `mods/backend_adapters.cpp`, `mods/mission_backend.h` | Extra quest rows: opaque per-row data for the quest tracker (`extension` role) and the quest log (`missionExtension`), activations back to the backend |
| `TQuestTrackerWidgetManager::setMissionBackend` | `gamewindow/questtrackerwidgetcontroller.*` | Tracker bind (`activateExtension(questFlagId, choice)`) |
| `TQuestLogController::setMissionBackend` | `gamewindow/questlogcontroller.*` | Quest log bind (`activateMissionExtension(choice)`) |
| `TMinimapRendererQuickItem::setExtraZoneMasks` | `qmlcomponents/minimaprendererquickitem.*` | Zone masks that are none of map.dat's subareas, drawn like a highlighted subarea |
| `TMapWindowController::setOverlayHover` | `gamewindow/mapwindowcontroller.*` | Chrome over the map reports the mouse; clicks/drags via `onMapWindowClicked` / `onMapWindowStartDrag` (ABI `pack_host->map_overlay_*`) |
| `TNpcDialogController::openExternalConversation` | `gamewindow/npcdialogcontroller.*` | A pack-driven NPC conversation (ABI `chat`); `shownChanged` |
| `TButtonBarController::addExtensionShortcut`, `TKeyboardShortcutHandler::addExtensionAction` | `gamewindow/buttonbarcontroller.*`, `input/keyboardshortcuthandler.*` | Display Shortcuts / hotkey actions a pack registers (ABI `register_button_bar_shortcut`, `register_hotkey_action`) |
| `TOptionsMenuDialogController::addExtensionPage` | `gamewindow/optionsmenudialogcontroller.*` | A pack's options page (ABI `pack_host->add_options_page`) |
| `set_world_frame_hooks` | `renderer/worldmaprenderprovider.*`, `mods/worldview.*` | `after_build` / `filter_light_map` on the live world's frame (weather, light-map filters) |
| `world_field_stack` | `renderer/worldframeview.*` | A map field's stack (CipSoft order, type ids + flags) for pack geometry |
| `sound` (`QtClientSoundApiV1`) | `sound/ambiencehold.*` | Hold CipSoft's ambience channel; play a bed / one-shots at the ambience volume |
| `set_texture_atlas_hint` | `mods/textureatlashint.*` | Raise the scene graph texture atlas before the first window |
| `set_effect_remap_hook`, `set_creature_decoration_hook`, `session->set_creature_hidden` | `effects/effectremap.*`, `creatures/creaturedecoration.*` | Draw another effect for a server id; sprites under / over creatures; hide a creature |
| `session->stored_fields_on_floor`, `waypoint_costs_snapshot`, `static_map_*` | `game/mapsession.*` | Map data peels for pack path finding and map.dat areas / subareas |
| `TSpeechBubbleOverlayQuickItem::setStatusBottomMargin` | Pack C++ (`THudLayout`, …) | Extra bottom inset so CIP status toasts clear pack chrome |

## Rules

1. **No product HUD in** `client/tibia/` — packs only.
2. Hooks are **CIP-neutral** (ids, backends, QML properties) — never reintroduce
   `ThingPtr` / `ItemPtr` keep-alives as the pack API.
3. Tip play path must not call into product pack symbols; packs plug hooks at load.
4. New pack needs → add a tip hook here and ratchet this doc; do not fork tip UX.
5. Do not add `TGameAction::object` / field UI `ThingPtr` / tip `UIWidget` — purity
   CI fails those regressions.

## Verify

```bash
python3 scripts/ci/check-cip-tip-purity.py
make stock desktop build          # tip without requiring pack
# the pack against the installed SDK alone, then the tip with it:
bash scripts/ci/check-pack-sdk-build.sh
make qtclient desktop install
```

See also: [mods.md](mods.md) (pack load / FS layout).
